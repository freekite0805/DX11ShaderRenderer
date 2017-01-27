
float4 main(uint id : SV_VertexID) : SV_POSITION
{
    float2 tex = float2((id << 1) & 2, id & 2);
    return float4(tex * float2(2,-2) + float2(-1,1), 0, 1);
}