#!/usr/bin/env python3
import argparse
import json
import os
import shlex
import subprocess
import sys
import time
from pathlib import Path


def run_command(command: str, timeout_seconds: int) -> subprocess.CompletedProcess:
	return subprocess.run(
		command,
		shell=True,
		capture_output=True,
		text=True,
		timeout=max(1, timeout_seconds),
	)


def write_health_json(path: str, payload: dict) -> None:
	if not path:
		return
	health_path = Path(path)
	health_path.parent.mkdir(parents=True, exist_ok=True)
	health_path.write_text(json.dumps(payload, indent=2), encoding="utf-8")


def build_template_candidates(repo_root: Path) -> list[str]:
	return [
		"{python} " + shlex.quote(str(repo_root / "infer.py")) + " --prompt {prompt_q} --output_glb {output_glb_q}",
		"{python} " + shlex.quote(str(repo_root / "infer_text_to_3d.py")) + " --prompt {prompt_q} --output_glb {output_glb_q}",
		"{python} " + shlex.quote(str(repo_root / "scripts" / "infer.py")) + " --prompt {prompt_q} --output_glb {output_glb_q}",
		"{python} " + shlex.quote(str(repo_root / "scripts" / "text_to_3d.py")) + " --prompt {prompt_q} --output_glb {output_glb_q}",
		"{python} " + shlex.quote(str(repo_root / "app.py")) + " --prompt {prompt_q} --save_path {output_glb_q}",
	]


def resolve_command_templates() -> tuple[list[str], str, bool]:
	env_template = os.environ.get("HUNYUAN3D_INFER_COMMAND_TEMPLATE", "").strip()
	repo_env = os.environ.get("HUNYUAN3D_REPO", "").strip()
	command_templates: list[str] = []

	if env_template:
		command_templates.append(env_template)

	if repo_env:
		repo_root = Path(repo_env)
		command_templates.extend(build_template_candidates(repo_root))

	return command_templates, repo_env, bool(env_template)


def self_check_backend(health_json_path: str) -> int:
	start_time = time.perf_counter()
	command_templates, repo_env, has_explicit_template = resolve_command_templates()
	repo_exists = bool(repo_env) and Path(repo_env).exists()
	mode = "explicit-template" if has_explicit_template else "repo-auto-discovery"

	if not command_templates:
		message = "Hunyuan3D backend is not configured. Set HUNYUAN3D_REPO or HUNYUAN3D_INFER_COMMAND_TEMPLATE."
		write_health_json(health_json_path, {
			"success": False,
			"mode": mode,
			"template_count": 0,
			"has_repo_env": bool(repo_env),
			"repo_exists": repo_exists,
			"has_explicit_template": has_explicit_template,
			"message": message,
			"duration_ms": int((time.perf_counter() - start_time) * 1000),
		})
		print(message, file=sys.stderr)
		return 2

	if repo_env and not repo_exists:
		message = f"Configured HUNYUAN3D_REPO does not exist: {repo_env}"
		write_health_json(health_json_path, {
			"success": False,
			"mode": mode,
			"template_count": len(command_templates),
			"has_repo_env": True,
			"repo_exists": False,
			"has_explicit_template": has_explicit_template,
			"message": message,
			"duration_ms": int((time.perf_counter() - start_time) * 1000),
		})
		print(message, file=sys.stderr)
		return 2

	message = f"Hunyuan3D backend self-check passed: mode={mode}, templates={len(command_templates)}"
	write_health_json(health_json_path, {
		"success": True,
		"mode": mode,
		"template_count": len(command_templates),
		"has_repo_env": bool(repo_env),
		"repo_exists": repo_exists,
		"has_explicit_template": has_explicit_template,
		"message": message,
		"duration_ms": int((time.perf_counter() - start_time) * 1000),
	})
	print(message)
	return 0


def main() -> int:
	parser = argparse.ArgumentParser(description="Hunyuan3D backend bridge for MyEngine")
	parser.add_argument("--prompt", default="")
	parser.add_argument("--output_glb", default="")
	parser.add_argument("--backend", default="hunyuan3d")
	parser.add_argument("--timeout_ms", type=int, default=300000)
	parser.add_argument("--self_check", action="store_true")
	parser.add_argument("--health_json", default="")
	args = parser.parse_args()

	if args.self_check:
		return self_check_backend(args.health_json)

	if not args.prompt or not args.output_glb:
		print("--prompt and --output_glb are required unless --self_check is used.", file=sys.stderr)
		return 2

	start_time = time.perf_counter()
	output_glb = Path(args.output_glb)
	output_glb.parent.mkdir(parents=True, exist_ok=True)
	if output_glb.exists():
		output_glb.unlink()

	python_exe = os.environ.get("HUNYUAN3D_PYTHON", sys.executable or "python")
	timeout_seconds = max(1, args.timeout_ms // 1000)
	prompt_q = shlex.quote(args.prompt)
	output_glb_q = shlex.quote(str(output_glb))
	python_q = shlex.quote(python_exe)

	command_templates, _, _ = resolve_command_templates()
	if not command_templates:
		message = (
			"Hunyuan3D backend is not configured. Set one of: "
			"HUNYUAN3D_INFER_COMMAND_TEMPLATE or HUNYUAN3D_REPO. "
			"Example template: {python} /path/to/infer.py --prompt {prompt_q} --output_glb {output_glb_q}"
		)
		write_health_json(args.health_json, {
			"success": False,
			"message": message,
			"template_count": 0,
			"duration_ms": int((time.perf_counter() - start_time) * 1000),
		})
		print(message, file=sys.stderr)
		return 2

	last_error = ""
	for template in command_templates:
		try:
			command = template.format(
				prompt=args.prompt,
				prompt_q=prompt_q,
				output_glb=str(output_glb),
				output_glb_q=output_glb_q,
				backend=args.backend,
				python=python_exe,
				python_q=python_q,
				timeout_ms=args.timeout_ms,
			)
		except Exception as exc:
			last_error = f"Template formatting failed: {exc}"
			continue

		try:
			proc = run_command(command, timeout_seconds)
		except subprocess.TimeoutExpired:
			last_error = f"Command timed out after {timeout_seconds}s: {command}"
			continue
		except Exception as exc:
			last_error = f"Command launch failed: {exc}"
			continue

		if proc.returncode != 0:
			details = (proc.stderr or proc.stdout or "").strip()
			last_error = f"Command failed ({proc.returncode}): {details}" if details else f"Command failed ({proc.returncode})"
			continue

		if output_glb.exists():
			write_health_json(args.health_json, {
				"success": True,
				"message": "Backend command completed and GLB exists.",
				"template_count": len(command_templates),
				"output_glb": str(output_glb),
				"duration_ms": int((time.perf_counter() - start_time) * 1000),
			})
			return 0

		last_error = "Command succeeded but output GLB was not produced at expected path."

	write_health_json(args.health_json, {
		"success": False,
		"message": last_error or "Hunyuan3D backend failed without details.",
		"template_count": len(command_templates),
		"output_glb": str(output_glb),
		"duration_ms": int((time.perf_counter() - start_time) * 1000),
	})
	print(last_error or "Hunyuan3D backend failed without details.", file=sys.stderr)
	return 3


if __name__ == "__main__":
	raise SystemExit(main())
