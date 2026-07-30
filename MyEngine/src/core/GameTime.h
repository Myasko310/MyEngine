#pragma once

namespace MyEngine {

    class GameTime {
    public:
        static void  Init();
        static void  Update();
        static float DeltaTime();
        static float TotalTime();

    private:
        static float s_DeltaTime;
        static float s_TotalTime;
        static long long s_LastTime;
    };

} // namespace MyEngine