// Reuse PP_SoftOutline's own DetectedStencil and EdgeMask. This is the same
// silhouette, width and additive compositing as the existing active-card branch.
// The original final intensity multiplier remains downstream of this expression.
float id = ceil(DetectedStencil);
float whiteMask = 1 - saturate(abs(id - 250));
float greenMask = 1 - saturate(abs(id - 251));
float redMask = 1 - saturate(abs(id - 252));
float3 color = whiteMask * float3(1,1,1)
    + greenMask * float3(0,1,0) + redMask * float3(1,0,0);
return Original + color * EdgeMask;
