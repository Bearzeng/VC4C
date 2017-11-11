__kernel void test_absdiff_char(__global char *srcA, __global char *srcB, __global uchar *dst)
{
    int  tid = get_global_id(0);

    char sA, sB;
    sA = srcA[tid];
    sB = srcB[tid];
    uchar dstVal = abs_diff(sA, sB);
	 dst[ tid ] = dstVal;
}

