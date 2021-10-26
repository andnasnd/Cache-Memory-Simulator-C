/* 
 * trans.c - Matrix transpose B = A^T
 *
 * Each transpose function must have a prototype of the form:
 * void trans(int M, int N, int A[N][M], int B[M][N]);
 *
 * A transpose function is evaluated by counting the number of misses
 * on a 1KB direct mapped cache with a block size of 32 bytes.
 */ 
#include <stdio.h>
#include "cachelab.h"

int is_transpose(int M, int N, int A[N][M], int B[M][N]);

/* 
 * You can define additional transpose functions below. We've defined
 * a simple one below to help you get started. 
 */ 
//Transpose 32x32 is done with block size 8, which neatly fits a cache line
//, attempting to avoid conflict miss. The tranpose is done in row-major order. 
void trans_32_32(int M, int N, int A[N][M], int B[M][N])
{
	int i;
	int block_i;
	int block_j; 
    const int block_size = 8;
	int tmp[block_size];

	for (block_j = 0; block_j < M; block_j+=block_size)
	{
		for (block_i = 0; block_i < N; block_i+=block_size)
		{
			for (i = block_i; i < block_i + block_size; i++)
			{
                int k = 0;
				for (; k < block_size; k++) {
                    tmp[k] = A[i][block_j + k];
                }
				for (k=0; k < block_size; k++) {
                    B[block_j+k][i] = tmp[k];
                }
			}
		}
	}
}
//My transpose 61x67 function works for 64x64, for block_size 4. 
void trans_64_64_test(int M, int N, int A[N][M], int B[M][N]) {
	int i;
	int block_i;
	int block_j; 
    const int block_size = 4;   
    int a; 
    int b; 
	for (block_j = 0; block_j < M; block_j += block_size) {
		for (block_i = 0; block_i < N; block_i += block_size) {
			for (a = block_j; (a < (block_j + block_size)) && (a < M); a++) {
                for (b = block_i; (b < (block_i + block_size)) && (b < N); b++) {
                    if (a != b) {
                       B[a][b] = A[b][a];  //transpose, similar to 32x32
                    } else {
                       i = A[b][b]; //diagonal, simply store in tmp,
                                    //no transpose
                    }
                }
                if (block_i == block_j) {
                    B[a][a] = i;    //retrieve diagonal from tmp 
                }
            }
        }
	}
}
void trans_64_64(int M, int N, int A[N][M], int B[M][N]) {
    int block_size=8;
    int tmp[8];
    int block_i;
    int block_j;
    int i;
    // 4 quadrants of 32 x 32
    // -----------
    // | Q1 | Q2 |
    // |----------
    // | Q3 | Q4 |
    // -----------
    // Q1
    {
	    for (block_j = 0; block_j < 32; block_j+=block_size)
	    {
		    for (block_i = 0; block_i < 32; block_i+=block_size)
		    {
			    for (i = block_i; i < block_i + block_size; i++)
			    {
                    int k = 0;
				    for (; k < block_size; k++) {
                        tmp[k] = A[i][block_j + k];
                    }
				    for (k=0; k < block_size; k++) {
                        B[block_j+k][i] = tmp[k];
                    }
			    }
		    }
	    }
    }
    // Transpose Q2 into Q3
    {
        for (int i = 32; i < 64; i++) {
            for (int j = 0; j < 32; j++) {
                B[i-32][j+32] = A[i][j];
            }
        }
    }
    // Transpose Q3 into Q2
    {
        for (int i = 0; i < 32; i++) {
            for (int j = 32; j < 64; j++) {
                B[i+32][j-32] = A[i][j];
            }
        }
    }
    // Q4
    {
	    for (int block_j = 32; block_j < 64; block_j+=block_size)
	    {
		    for (int block_i = 32; block_i < 64; block_i+=block_size)
		    {
			    for (int i = block_i; i < block_i + block_size; i++)
			    {
                    int k = 0;
				    for (; k < block_size; k++) {
                        tmp[k] = A[i][block_j + k];
                    }
				    for (k=0; k < block_size; k++) {
                        B[block_j+k][i] = tmp[k];
                    }
			    }
		    }
	    }
    }
}

//For this function, I used a similar approach as 32x32, but used block_size 18
//to avoid conflict miss. 
void trans_61_67(int M, int N, int A[N][M], int B[M][N]) {
	int i;
	int block_i;
	int block_j; 
    const int block_size = 18;    //tried block size 16, 17, 18, 19, 20
    int a; 
    int b; 
	for (block_j = 0; block_j < M; block_j += block_size) {
		for (block_i = 0; block_i < N; block_i += block_size) {
			for (a = block_j; (a < (block_j + block_size)) && (a < M); a++) {
                for (b = block_i; (b < (block_i + block_size)) && (b < N); b++) {
                    if (a != b) {
                       B[a][b] = A[b][a];  //transpose, similar to 32x32
                    } else {
                       i = A[b][b]; //diagonal, simply store in tmp, no transpose
                    }
                }
                if (block_i == block_j) {
                    B[a][a] = i;    //retrieve diagonal from tmp 
                }
            }
        }
	}
}

/* 
 * trans - A simple baseline transpose function, not optimized for the cache.
 */
char trans_desc[] = "Simple row-wise scan transpose";
void trans(int M, int N, int A[N][M], int B[M][N])
{
    int i, j, tmp;

    for (i = 0; i < N; i++) {
        for (j = 0; j < M; j++) {
            tmp = A[i][j];
            B[j][i] = tmp;
        }
    }    
  
}

/* 
 * transpose_submit - This is the solution transpose function that you
 *     will be graded on for Part B of the assignment. Do not change
 *     the description string "Transpose submission", as the driver
 *     searches for that string to identify the transpose function to
 *     be graded. 
 */
char transpose_submit_desc[] = "Transpose submission";
void transpose_submit(int M, int N, int A[N][M], int B[M][N])
{
	if (M == 32 && N == 32) {
		trans_32_32(M,N,A,B);
	} else if (M == 64 && N == 64) {
        trans_64_64_test(M,N,A,B);
    } else if (M == 61 && N == 67) {
        trans_61_67(M,N,A,B);
    }
}

/*
 * registerFunctions - This function registers your transpose
 *     functions with the driver.  At runtime, the driver will
 *     evaluate each of the registered functions and summarize their
 *     performance. This is a handy way to experiment with different
 *     transpose strategies.
 */

void registerFunctions()
{
    /* Register your solution function */
    registerTransFunction(transpose_submit, transpose_submit_desc); 

    /* Register any additional transpose functions */
    registerTransFunction(trans, trans_desc); 

}

/* 
 * is_transpose - This helper function checks if B is the transpose of
 *     A. You can check the correctness of your transpose by calling
 *     it before returning from the transpose function.
 */
int is_transpose(int M, int N, int A[N][M], int B[M][N])
{
    int i, j;

    for (i = 0; i < N; i++) {
        for (j = 0; j < M; ++j) {
            if (A[i][j] != B[j][i]) {
                return 0;
            }
        }
    }
    return 1;
}

