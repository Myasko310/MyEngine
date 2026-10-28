#!/usr/bin/env python3
import argparse
import json
import os
import subprocess
import sys
import time
from pathlib import Path
from typing import Optional


def write_result(path: Path, payload: dict) -> int:
	path.parent.mkdir(parents=True, exist_ok=True)
	path.write_text(json.dumps(payload, indent=2), encoding="utf-8")
	return 0 if payload.get("success", False) else 1


def main() -> int:
	parser = argparse.ArgumentParser(description="MyEngine local text-to-3D runner contract")
	parser.add_argument("--prompt", required=True)
	parser.add_argument("--output_dir", required=True)
	parser.add_argument("--output_name", required=True)
	parser.add_argument("--backend", default="hunyuan3d")
	parser.add_argument("--result_json", required=True)
	parser.add_argument("--timeout_ms", type=int, default=300000)
	parser.add_argument("--require_glb", type=int, default=1)
	parser.add_argument("--retry_count", type=int, default=1)
	parser.add_argument("--retry_backoff_ms", type=int, default=250)
	args = parser.parse_args()

	start_time = time.perf_counter()
	output_dir = Path(args.output_dir)
	output_dir.mkdir(parents=True, exist_ok=True)
	result_json = Path(args.result_json)

	glb_candidate = output_dir / f"{args.output_name}.glb"

	command_template = os.environ.get("MYENGINE_TEXT_TO_3D_COMMAND", "").strip()
	used_default_template = False
	fallback_template_path = "tools/ai/hunyuan3d_command_template.ps1"
	if not command_template:
		if os.name == "nt" or sys.platform.startswith("win"):
			command_template = (
				"powershell -ExecutionPolicy Bypass -File tools/ai/hunyuan3d_command_template.ps1 "
				"-Prompt \"{prompt}\" -OutputDir \"{output_dir}\" -OutputName \"{output_name}\" "
				"-Backend \"{backend}\" -TimeoutMs {timeout_ms}"
			)
			used_default_template = True
		else:
			return write_result(result_json, {
				"success": False,
				"message": (
					"No text-to-3D backend command configured. Set MYENGINE_TEXT_TO_3D_COMMAND "
					"to a command template that writes a GLB. Supported placeholders: "
					"{prompt}, {output_dir}, {output_name}, {backend}, {timeout_ms}."
				),
				"generated_files": [],
				"backend": args.backend,
				"used_default_template": False,
				"fallback_template_path": fallback_template_path,
				"command_template_source": "missing"
			})

	command = command_template.format(
		prompt=args.prompt,
		output_dir=str(output_dir),
		output_name=args.output_name,
		backend=args.backend,
		timeout_ms=args.timeout_ms,
	)

	command_template_source = "env" if os.environ.get("MYENGINE_TEXT_TO_3D_COMMAND", "").strip() else "default"
	configured_attempts = max(1, args.retry_count)
	max_attempts = max(configured_attempts, 2 if used_default_template else configured_attempts)
	retry_backoff_ms = max(0, args.retry_backoff_ms)
	attempts: list[dict] = []
	proc: Optional[subprocess.CompletedProcess] = None
	last_launch_error = ""

	for attempt_index in range(max_attempts):
		attempt_no = attempt_index + 1
		attempt_start = time.perf_counter()
		try:
			proc = subprocess.run(
				command,
				shell=True,
				capture_output=True,
				text=True,
				timeout=max(1, args.timeout_ms // 1000),
			)
			attempts.append({
				"attempt": attempt_no,
				"exit_code": proc.returncode,
				"stderr": proc.stderr,
				"stdout": proc.stdout,
				"duration_ms": int((time.perf_counter() - attempt_start) * 1000),
			})
			if proc.returncode == 0:
				break
			if attempt_no < max_attempts and retry_backoff_ms > 0:
				time.sleep(retry_backoff_ms / 1000.0)
		except subprocess.TimeoutExpired:
			attempts.append({
				"attempt": attempt_no,
				"timeout": True,
				"duration_ms": int((time.perf_counter() - attempt_start) * 1000),
			})
			last_launch_error = "Text-to-3D backend command timed out."
			proc = None
			if attempt_no < max_attempts and retry_backoff_ms > 0:
				time.sleep(retry_backoff_ms / 1000.0)
		except Exception as exc:
			attempts.append({
				"attempt": attempt_no,
				"launch_error": str(exc),
				"duration_ms": int((time.perf_counter() - attempt_start) * 1000),
			})
			last_launch_error = f"Failed to execute text-to-3D backend command: {exc}"
			proc = None
			if attempt_no < max_attempts and retry_backoff_ms > 0:
				time.sleep(retry_backoff_ms / 1000.0)

	if proc is None and attempts:
		message = last_launch_error or "Text-to-3D backend command failed to launch."
		return write_result(result_json, {
			"success": False,
			"message": message,
			"generated_files": [],
			"backend": args.backend,
			"used_default_template": used_default_template,
			"fallback_template_path": fallback_template_path,
			"command_template_source": command_template_source,
			"command": command,
			"attempt_count": len(attempts),
			"attempts": attempts,
			"retry_count": max_attempts,
			"retry_backoff_ms": retry_backoff_ms,
			"total_duration_ms": int((time.perf_counter() - start_time) * 1000),
		})

	if proc is None:
		return write_result(result_json, {
			"success": False,
			"message": "Text-to-3D backend command failed without process result.",
			"generated_files": [],
			"backend": args.backend,
			"used_default_template": used_default_template,
			"fallback_template_path": fallback_template_path,
			"command_template_source": command_template_source,
			"command": command,
			"attempt_count": len(attempts),
			"attempts": attempts,
			"retry_count": max_attempts,
			"retry_backoff_ms": retry_backoff_ms,
			"total_duration_ms": int((time.perf_counter() - start_time) * 1000),
		})

	if proc.returncode != 0:
		message = proc.stderr.strip() or proc.stdout.strip() or "Text-to-3D backend returned non-zero exit code."
		if used_default_template:
			message = "Default Hunyuan3D template command failed. " + message
		return write_result(result_json, {
			"success": False,
			"message": message,
			"generated_files": [],
			"backend": args.backend,
			"used_default_template": used_default_template,
			"fallback_template_path": fallback_template_path,
			"command_template_source": command_template_source,
			"command": command,
			"exit_code": proc.returncode,
			"stderr": proc.stderr,
			"stdout": proc.stdout,
			"attempt_count": len(attempts),
			"attempts": attempts,
			"retry_count": max_attempts,
			"retry_backoff_ms": retry_backoff_ms,
			"total_duration_ms": int((time.perf_counter() - start_time) * 1000),
		})

	if args.require_glb and not glb_candidate.exists():
		return write_result(result_json, {
			"success": False,
			"message": f"Backend command succeeded but GLB was not found at expected path: {glb_candidate}",
			"generated_files": [],
			"backend": args.backend,
			"used_default_template": used_default_template,
			"fallback_template_path": fallback_template_path,
			"command_template_source": command_template_source,
			"command": command,
			"exit_code": proc.returncode,
			"stderr": proc.stderr,
			"stdout": proc.stdout,
			"attempt_count": len(attempts),
			"attempts": attempts,
			"retry_count": max_attempts,
			"retry_backoff_ms": retry_backoff_ms,
			"total_duration_ms": int((time.perf_counter() - start_time) * 1000),
		})

	generated_files = []
	if glb_candidate.exists():
		generated_files.append(str(glb_candidate))

	return write_result(result_json, {
		"success": True,
		"message": "Text-to-3D generation completed.",
		"glb_path": str(glb_candidate) if glb_candidate.exists() else "",
		"generated_files": generated_files,
		"backend": args.backend,
		"used_default_template": used_default_template,
		"fallback_template_path": fallback_template_path,
		"command_template_source": command_template_source,
		"command": command,
		"exit_code": proc.returncode,
		"stderr": proc.stderr,
		"stdout": proc.stdout,
		"attempt_count": len(attempts),
		"attempts": attempts,
		"retry_count": max_attempts,
		"retry_backoff_ms": retry_backoff_ms,
		"total_duration_ms": int((time.perf_counter() - start_time) * 1000),
	})


if __name__ == "__main__":
	raise SystemExit(main())
