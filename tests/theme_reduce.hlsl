/*
 * theme_reduce.hlsl -- T6 surface-color reduction compute shader (cs_5_0).
 *
 * One Dispatch per captured frame; one thread group per sampled surface. Each group integer-sums the
 * RGB of every pixel in that surface's capture-space rectangle into groupshared accumulators (via
 * InterlockedAdd, so the result is order-independent and bit-for-bit deterministic regardless of how
 * the GPU schedules the group's threads), then thread 0 writes the (sumR, sumG, sumB, pixelCount) tuple
 * to gOut[frameSlot*THEME_MAX_SURF + surface]. The CPU divides sum/count to recover the mean color.
 *
 * A B8G8R8A8_UNORM SRV is presented to HLSL as logical .rgba, so .r is red; Load on a UNORM texel
 * returns exactly byte/255, and (uint)(c*255+0.5) recovers the original byte exactly -- the sum is an
 * exact integer sum of 0..255 channel bytes, no float accumulation error.
 */

#define THEME_MAX_SURF 8u

cbuffer ThemeReduceParams : register(b0)
{
    uint  gSurfCount;   /* surfaces actually populated this run (<= THEME_MAX_SURF)        */
    uint  gFrameSlot;   /* output base index = gFrameSlot * THEME_MAX_SURF                 */
    uint  gPad0;
    uint  gPad1;
    uint4 gRects[THEME_MAX_SURF]; /* per surface: .xy = capture-space origin, .zw = extent */
};

Texture2D<float4>          gFrame : register(t0);
RWStructuredBuffer<uint4>  gOut   : register(u0);

groupshared uint gsR;
groupshared uint gsG;
groupshared uint gsB;
groupshared uint gsN;

[numthreads(8, 8, 1)]
void main(uint3 gid : SV_GroupID, uint3 tid : SV_GroupThreadID, uint gi : SV_GroupIndex)
{
    if (0u == gi)
    {
        gsR = 0u;
        gsG = 0u;
        gsB = 0u;
        gsN = 0u;
    }
    GroupMemoryBarrierWithGroupSync();

    uint s = gid.x;
    if (s < gSurfCount)
    {
        uint ox = gRects[s].x;
        uint oy = gRects[s].y;
        uint w  = gRects[s].z;
        uint h  = gRects[s].w;
        uint yy;
        uint xx;
        for (yy = tid.y; yy < h; yy += 8u)
        {
            for (xx = tid.x; xx < w; xx += 8u)
            {
                float4 c = gFrame.Load(int3((int)(ox + xx), (int)(oy + yy), 0));
                InterlockedAdd(gsR, (uint)(c.r * 255.0 + 0.5));
                InterlockedAdd(gsG, (uint)(c.g * 255.0 + 0.5));
                InterlockedAdd(gsB, (uint)(c.b * 255.0 + 0.5));
                InterlockedAdd(gsN, 1u);
            }
        }
    }

    GroupMemoryBarrierWithGroupSync();
    if (0u == gi)
    {
        gOut[gFrameSlot * THEME_MAX_SURF + s] = uint4(gsR, gsG, gsB, gsN);
    }
}
