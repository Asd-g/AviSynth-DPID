#include <algorithm>
#include <cmath>

#include "avisynth.h"

class dpid : public GenericVideoFilter
{
    PClip clip;
    double lambda[4];
    double src_left[4];
    double src_top[4];
    int process[4];
    int64_t read_chromaloc;
    double hSubS;
    double vSubS;
    bool v8;
    bool colsp;

    template <typename T>
    PVideoFrame dpidProcess(PVideoFrame& dst, PVideoFrame& src1, PVideoFrame& src2, IScriptEnvironment* env);
    PVideoFrame(dpid::* proc)(PVideoFrame& dst, PVideoFrame& src1, PVideoFrame& src2, IScriptEnvironment* env);

public:
    dpid(PClip _child, PClip _clip, int width, int height, double lambdaY, double lambdaU, double lamdbaV, double lamdbaA, double src_leftY, double src_leftU, double src_leftV, double src_leftA,
        double src_topY, double src_topU, double src_topV, double src_topA, int _read_chromaloc, int y, int u, int v, int a, IScriptEnvironment* env);

    int __stdcall SetCacheHints(int cachehints, int frame_range) override
    {
        return cachehints == CACHE_GET_MTMODE ? MT_MULTI_INSTANCE : 0;
    }

    PVideoFrame __stdcall GetFrame(int n, IScriptEnvironment* env) override;
};

AVS_FORCEINLINE double contribution(double f, double x, double y, double sx, double ex, double sy, double ey)
{

    if (x < sx)
        f *= 1.0 - (sx - x);

    if ((x + 1.0) > ex)
        f *= ex - x;

    if (y < sy)
        f *= 1.0 - (sy - y);

    if ((y + 1.0) > ey)
        f *= ey - y;

    return f;
}

template <typename T>
PVideoFrame dpid::dpidProcess(PVideoFrame& dst, PVideoFrame& src1, PVideoFrame& src2, IScriptEnvironment* env)
{
    const int planes_y[4] = {PLANAR_Y, PLANAR_U, PLANAR_V, PLANAR_A};
    const int planes_r[4] = {PLANAR_R, PLANAR_G, PLANAR_B, PLANAR_A};
    const int* planes = (vi.IsRGB()) ? planes_r : planes_y;

    for (int i = 0; i < vi.NumComponents(); ++i)
    {
        if (process[i] == 3)
        {
            const int src_stride = src1->GetPitch(planes[i]) / sizeof(T);
            const int down_stride = src2->GetPitch(planes[i]) / sizeof(T);
            const int dst_stride = dst->GetPitch(planes[i]) / sizeof(T);
            const int src_w = src1->GetRowSize(planes[i]) / sizeof(T);
            const int dst_w = src2->GetRowSize(planes[i]) / sizeof(T);
            const int src_h = src1->GetHeight(planes[i]);
            const int dst_h = src2->GetHeight(planes[i]);
            const T* srcp = reinterpret_cast<const T*>(src1->GetReadPtr(planes[i]));
            const T* downp = reinterpret_cast<const T*>(src2->GetReadPtr(planes[i]));
            T* dstp = reinterpret_cast<T*>(dst->GetWritePtr(planes[i]));

            bool check = i != 0 && colsp;

            const int chromaLocation = [&]() {
                if (check)
                {
                    if (read_chromaloc == -1)
                    {
                        const AVSMap* props = env->getFramePropsRO(src2);
                        return (env->propNumElements(props, "_ChromaLocation") > 0) ? static_cast<int>(env->propGetInt(props, "_ChromaLocation", 0, nullptr)) : 0;
                    }
                    else
                        return static_cast<int>(read_chromaloc);
                }
                else
                    return 0;
            }();

            const double _src_left = [&]() {
                if (check)
                {
                    const double hCPlace = (chromaLocation == 0 || chromaLocation == 2 || chromaLocation == 4) ? (0.5 - hSubS / 2) : 0.0;
                    const double hScale = static_cast<double>(dst_w) / static_cast<double>(src_w);

                    return ((src_left[i] - hCPlace) * hScale + hCPlace) / hScale / hSubS;
                }
                else
                    return src_left[i];
            }();

            const double _src_top = [&]() {
                if (check)
                {
                    const double vCPlace = (chromaLocation == 2 || chromaLocation == 3) ? (0.5 - vSubS / 2) : ((chromaLocation == 4 || chromaLocation == 5) ? (vSubS / 2 - 0.5) : 0.0);
                    const double vScale = static_cast<double>(dst_h) / static_cast<double>(src_h);

                    return ((src_top[i] - vCPlace) * vScale + vCPlace) / vScale / vSubS;
                }
                else
                    return src_top[i];
            }();

            const double scale_x = static_cast<double>(src_w) / dst_w;
            const double scale_y = static_cast<double>(src_h) / dst_h;

            for (int outer_y = 0; outer_y < dst_h; ++outer_y)
            {
                for (int outer_x = 0; outer_x < dst_w; ++outer_x)
                {
                    // avg = RemoveGrain(down, 11)
                    double avg = 0.0;

                    for (int inner_y = -1; inner_y <= 1; ++inner_y)
                    {
                        const int y = std::clamp(outer_y + inner_y, 0, dst_h - 1);

                        for (int inner_x = -1; inner_x <= 1; ++inner_x)
                        {
                            const int x = std::clamp(outer_x + inner_x, 0, dst_w - 1);

                            avg += downp[y * down_stride + x] * (2 - std::abs(inner_y)) * (2 - std::abs(inner_x));
                        }
                    }

                    avg /= 16.0;

                    // Dpid
                    const double sx = std::clamp(outer_x * scale_x + _src_left, 0.0, static_cast<double>(src_w));
                    const double ex = std::clamp((outer_x + 1) * scale_x + _src_left, 0.0, static_cast<double>(src_w));
                    const double sy = std::clamp(outer_y * scale_y + _src_top, 0.0, static_cast<double>(src_h));
                    const double ey = std::clamp((outer_y + 1) * scale_y + _src_top, 0.0, static_cast<double>(src_h));

                    const int sxr = static_cast<int>(std::floor(sx));
                    const int exr = static_cast<int>(std::ceil(ex));
                    const int syr = static_cast<int>(std::floor(sy));
                    const int eyr = static_cast<int>(std::ceil(ey));

                    double sum_pixel = 0.0;
                    double sum_weight = 0.0;

                    for (int inner_y = syr; inner_y < eyr; ++inner_y)
                    {
                        for (int inner_x = sxr; inner_x < exr; ++inner_x)
                        {
                            T pixel = srcp[inner_y * src_stride + inner_x];
                            const double weight = contribution(std::pow(std::abs(avg - static_cast<double>(pixel)), lambda[i]), static_cast<double>(inner_x), static_cast<double>(inner_y), sx, ex, sy, ey);

                            sum_pixel += weight * pixel;
                            sum_weight += weight;
                        }
                    }

                    dstp[outer_y * dst_stride + outer_x] = static_cast<T>((sum_weight == 0.0) ? avg : sum_pixel / sum_weight);
                }
            }
        }
        else if (process[i] == 2)
            env->BitBlt(dst->GetWritePtr(planes[i]), dst->GetPitch(planes[i]), src2->GetReadPtr(planes[i]), src2->GetPitch(planes[i]), src2->GetRowSize(planes[i]), src2->GetHeight(planes[i]));
    }

    return dst;
}

dpid::dpid(PClip _child, PClip _clip, int width, int height, double lambdaY, double lambdaU, double lambdaV, double lambdaA, double src_leftY, double src_leftU, double src_leftV, double src_leftA,
    double src_topY, double src_topU, double src_topV, double src_topA, int _read_chromaloc, int y, int u, int v, int a, IScriptEnvironment* env)
    : GenericVideoFilter(_child), clip(_clip), lambda{ lambdaY, lambdaU, lambdaV, lambdaA }, src_left{ src_leftY, src_leftU, src_leftV, src_leftA }, src_top{ src_topY, src_topU, src_topV, src_topA },
    read_chromaloc(_read_chromaloc)
{
    if (width == -1365)
    {
        if (!vi.IsPlanar())
            env->ThrowError("DPIDraw: only planar formats are supported.");

        const VideoInfo& vi1 = clip->GetVideoInfo();

        if (!vi.IsSameColorspace(vi1))
            env->ThrowError("DPIDraw: clip and clip1 doesn't have the same colorspace.");
        if (vi.num_frames != vi1.num_frames)
            env->ThrowError("DPIDraw: clip and clip1 must have the same number of frames.");
        for (int i = 0; i < 4; ++i)
        {
            if (lambda[i] < 0.1)
                env->ThrowError("DPIDraw: lambda must be greater than 0.1.");
        }
        if (read_chromaloc < -1 || read_chromaloc > 5)
            env->ThrowError("DPIDraw: cloc must be between -1..5.");
        if (y < 1 || y > 3)
            env->ThrowError("DPIDraw: y must be between 1..3.");
        if (u < 1 || u > 3)
            env->ThrowError("DPIDraw: u must be between 1..3.");
        if (v < 1 || v > 3)
            env->ThrowError("DPIDraw: v must be between 1..3.");

        vi.width = vi1.width;
        vi.height = vi1.height;
    }
    else
    {
        if (!vi.IsPlanar())
            env->ThrowError("DPID: only planar formats are supported.");
        for (int i = 0; i < 4; ++i)
        {
            if (lambda[i] < 0.1)
                env->ThrowError("DPID: lambda must be greater than 0.1.");
        }
        if (read_chromaloc < -1 || read_chromaloc > 5)
            env->ThrowError("DPID: cloc must be between -1..5.");

        vi.width = width;
        vi.height = height;
    }

    const int p[4] = { y, u, v, a };

    for (int i = 0; i < vi.NumComponents(); ++i)
    {
        if (vi.IsRGB())
            process[i] = (i < 4) ? 3 : a;
        else
        {
            switch (p[i])
            {
                case 3: process[i] = 3; break;
                case 2: process[i] = 2; break;
                default: process[i] = 1; break;
            }
        }
    }

    if (vi.NumComponents() > 1 && !vi.IsRGB())
    {
        hSubS = static_cast<double>(1 << vi.GetPlaneWidthSubsampling(PLANAR_U));
        vSubS = static_cast<double>(1 << vi.GetPlaneHeightSubsampling(PLANAR_U));
    }
    else
    {
        hSubS = 0.0;
        vSubS = 0.0;
    }

    switch (vi.ComponentSize())
    {
        case 1: proc = &dpid::dpidProcess<uint8_t>; break;
        case 2: proc = &dpid::dpidProcess<uint16_t>; break;
        default: proc = &dpid::dpidProcess<float>; break;
    }

    v8 = env->FunctionExists("propShow");
    if (!v8)
        read_chromaloc = (read_chromaloc == -1) ? 0 : read_chromaloc;

    colsp = !vi.IsRGB() && !vi.Is444();
}

PVideoFrame __stdcall dpid::GetFrame(int n, IScriptEnvironment* env)
{
    PVideoFrame src = child->GetFrame(n, env);
    PVideoFrame src1 = clip->GetFrame(n, env);
    PVideoFrame dst = (v8) ? env->NewVideoFrameP(vi, &src1) : env->NewVideoFrame(vi);
    return (this->*proc)(dst, src, src1, env);
}

AVSValue __cdecl Create_DPID(AVSValue args, void* user_data, IScriptEnvironment* env)
{
    enum { Clip, Width, Height, LambdaY, LambdaU, LambdaV, LambdaA, Src_leftY, Src_leftU, Src_leftV, Src_leftA, Src_topY, Src_topU, Src_topV, Src_topA, Cloc };

    PClip clip = args[Clip].AsClip();
    const int width = args[Width].AsInt();
    const int height = args[Height].AsInt();
    const float lambdaY = args[LambdaY].AsFloatf(1.0f);
    const float lambdaU = args[LambdaU].AsFloatf(lambdaY);
    const float src_leftY = args[Src_leftY].AsFloatf(0.0f);
    const float src_leftU = args[Src_leftU].AsFloatf(src_leftY);
    const float src_topY = args[Src_topY].AsFloatf(0.0f);
    const float src_topU = args[Src_topU].AsFloatf(src_topY);

    const char* names[5] = { NULL, NULL, NULL, "src_left", "src_top" };
    AVSValue args_[5] = { clip, width, height, src_leftY, src_topY };
    PClip clip1 = env->Invoke("z_BilinearResize", AVSValue(args_, 5), names).AsClip();

    return new dpid(
        clip,
        clip1,
        width,
        height,
        lambdaY,
        lambdaU,
        args[LambdaV].AsFloatf(lambdaU),
        args[LambdaA].AsFloatf(lambdaY),
        src_leftY,
        src_leftU,
        args[Src_leftV].AsFloatf(src_leftU),
        args[Src_leftA].AsFloatf(src_leftY),
        src_topY,
        src_topU,
        args[Src_topV].AsFloatf(src_topU),
        args[Src_topA].AsFloatf(src_topY),
        args[Cloc].AsInt(-1),
        3,
        3,
        3,
        3,
        env);
}

AVSValue __cdecl Create_DPIDraw(AVSValue args, void* user_data, IScriptEnvironment* env)
{
    enum { Clip, Clip1, LambdaY, LambdaU, LambdaV, LambdaA, Src_leftY, Src_leftU, Src_leftV, Src_leftA, Src_topY, Src_topU, Src_topV, Src_topA, Cloc, Y, U, V, A };

    const float lambdaY = args[LambdaY].AsFloatf(1.0f);
    const float lambdaU = args[LambdaU].AsFloatf(lambdaY);
    const float src_leftY = args[Src_leftY].AsFloatf(0.0f);
    const float src_leftU = args[Src_leftU].AsFloatf(src_leftY);
    const float src_topY = args[Src_topY].AsFloatf(0.0f);
    const float src_topU = args[Src_topU].AsFloatf(src_topY);

    return new dpid(
        args[Clip].AsClip(),
        args[Clip1].AsClip(),
        -1365,
        -1365,
        lambdaY,
        lambdaU,
        args[LambdaV].AsFloatf(lambdaU),
        args[LambdaA].AsFloatf(lambdaY),
        src_leftY,
        src_leftU,
        args[Src_leftV].AsFloatf(src_leftU),
        args[Src_leftA].AsFloatf(src_leftY),
        src_topY,
        src_topU,
        args[Src_topV].AsFloatf(src_topU),
        args[Src_topA].AsFloatf(src_topY),
        args[Cloc].AsInt(-1),
        args[Y].AsInt(3),
        args[U].AsInt(3),
        args[V].AsInt(3),
        args[A].AsInt(1),
        env);
}

const AVS_Linkage* AVS_linkage;

extern "C" __declspec(dllexport)
const char* __stdcall AvisynthPluginInit3(IScriptEnvironment * env, const AVS_Linkage* const vectors)
{
    AVS_linkage = vectors;

    env->AddFunction("DPID", "cii[lambdaY]f[lambdaU]f[lambdaV]f[lambdaA]f[src_leftY]f[src_leftU]f[src_leftV]f[src_leftA]f[src_topY]f[src_topU]f[src_topV]f[src_topA]f[cloc]i", Create_DPID, 0);
    env->AddFunction("DPIDraw", "cc[lambdaY]f[lambdaU]f[lambdaV]f[lambdaA]f[src_leftY]f[src_leftU]f[src_leftV]f[src_leftA]f[src_topY]f[src_topU]f[src_topV]f[src_topA]f[cloc]i[y]i[u]i[v]i[a]i", Create_DPIDraw, 0);

    return "DPID";
}
