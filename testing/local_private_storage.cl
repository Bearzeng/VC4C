

//Code sample adopted from @harleyzhang (taken from: https://github.com/doe300/VC4C/issues/30) 
__kernel void test_local_storage(__global int* in, __global int* out)
{
	size_t gid = get_global_id(0);
	uchar lid = get_local_id(0);
	
	__local int loc[12];

	loc[lid] = in[gid];
	barrier(CLK_LOCAL_MEM_FENCE);
	loc[lid] = loc[lid] + loc[lid + 1];
	out[gid] = loc[lid];
}


__kernel void test_private_storage(__global int* in, __global int* out)
{
	size_t gid = get_global_id(0);
	uchar lid = get_local_id(0);
	
	__private int loc[12];

	loc[0] = in[gid];
	loc[1] = in[gid] + 4;
	loc[2] = in[gid] + 5;
	loc[3] = in[gid] + 6;
	loc[4] = in[gid] + 7;
	loc[5] = in[gid] + 8;
	loc[6] = in[gid] + 9;
	loc[7] = in[gid] + 10;
	loc[8] = in[gid] + 11;
	loc[9] = in[gid] + 12;
	loc[10] = in[gid] + 13;
	loc[11] = in[gid] + 14;
	
	barrier(CLK_LOCAL_MEM_FENCE);
	loc[0] = loc[0] + loc[lid];
	out[gid] = loc[0];
}