#include <iostream>
#include <math.h>
#include <opencv2/core.hpp>
#include <opencv2/imgcodecs.hpp>
#include "utils.h"
#include "intrin-wrapper.h"
#include <immintrin.h>


// Headers for intrinsics
#ifdef __SSE__
#include <xmmintrin.h>
#endif
#ifdef __SSE2__
#include <emmintrin.h>
#endif
#ifdef __AVX__
#include <immintrin.h>
#endif

using namespace std;
using namespace cv;

namespace
{
    //! [burningship-simd]
    void simdburningship(int*pixelMatrix, int rows, int cols, const float x1, const float y1, const float scaleX, const float scaleY)
    {

        // define constants out here
        const __m256d _two = _mm256_set1_pd(2.0);
        const __m256d _four = _mm256_set1_pd(4.0);
        const __m256d _zero = _mm256_set1_pd(0.0);
        const __m256 _twofivefive = _mm256_set1_ps(255.0);
        const __m256d sign_bit = _mm256_castsi256_pd(_mm256_set1_epi64x(0x8000000000000000));

        // for getting the cr values
        const __m256d _x_scale = _mm256_set1_pd(scaleX);
        const __m256d _x1 = _mm256_set1_pd(x1);


        for (int i = 0; i < rows; i++)
        {
            // ci = y0 = i / scaleY + y1
            double y0 = i / scaleY + y1;
            __m256d ci_vec = _mm256_set1_pd(y0);

            // increment in groups of 4 as we will be putting 4 packed doubles in the register
            for (int j = 0; j < cols; j += 4)
            {
                __m256d _j = _mm256_set_pd(j,j+1,j+2,j+3);

                // get cr = j / scaleX + x1
                __m256d _cr_inter = _mm256_div_pd(_j,_x_scale);
                __m256d cr_vec = _mm256_add_pd(_cr_inter,_x1);


                // for each pixel in our image, figure out what coords that pixel
                // corresponds to in the domain of our problem
                // double x0 = j / scaleX + x1;
                //double y0 = i / scaleY + y1;


                // real is the x-axis
                // double cr = x0;
                // imaginary is the y-axis
               // double ci = y0;


                // instead of calling separate functions, do everything in here
                // it will enable me to store the vectorized result
                // wont have to worry about how to return the vectorized result??
                int max_iter = 500;

                __m256d zr = _mm256_set1_pd(0.0); // 4 copies of 0.0
                __m256d zi = _mm256_set1_pd(0.0);
                __m256d re = _mm256_set1_pd(0.0);
                __m256d im = _mm256_set1_pd(0.0);

                // make a vector to store copies of the starting c values of the loop
                // __m256d cr_vec = _mm256_set1_pd(cr); // 4 copies of cr
                // __m256d ci_vec = _mm256_set1_pd(ci); // 4 copies of ci


                //double my_array[4];
                //_mm256_storeu_pd(my_array, cr_vec);
                //printf("Vector contents: (row:%d col:%d) %f %f %f %f\n", i,j, my_array[3], my_array[2], my_array[1], my_array[0]);

                // also make vectors to store the intermediate counter for num iterations taken
                __m256i counter_vec = _mm256_set1_epi32(0); // initially, each pixel has 0 iterations --- must be INTEGER

                // try this later, maybe enable me to exit early?
                __m256i max_vec = _mm256_set1_epi32(max_iter); // epi32 for INTEGER

                // ESCAPE TIME LOOP
                // start the main loop for this pixel
                for (int t = 0; t < max_iter; t++) {

                    // first generate the condition 
                    // (zr * zr + zi * zi) > 4.0
                    __m256d zr2 = _mm256_mul_pd(zr, zr);
                    __m256d zi2 = _mm256_mul_pd(zi, zi);
                    __m256d sum = _mm256_add_pd(zr2, zi2);

                    // create a mask to check each element in the vector for the condition
                    __m256d mask = _mm256_cmp_pd(sum, _four, _CMP_LE_OQ);
                        // sum <= 4
                        // produces [0,0,0,1] for example for (false, false, false, true)

                    // check if all of the four pixels sum's are NOT less than zero (they are all false)
                    if (_mm256_testz_pd(mask, mask)) {

                        // if [0,0,0,0], then no more iterations neeeded 
                        // the counter vec is OK to be used by the output
                        break;
                    }

                    // otherwise, add the mask to the counter vec
                    // pixels which have "escaped" by now should not accumulate more num_iterations
                    // pixels which have not "escaped" by now should +1 to the num of iterations needed to satisfy
                    counter_vec = _mm256_add_epi32(counter_vec, _mm256_castpd_si256(mask));
                    // ^ mask needs to be casted from integer to 32 bit
                    // omg, then cast back to int


                    // *********************
                    // then, do the fractal computation for this single iteration
                    // z = abs(z * z) + c;
                    // re = zr * zr - zi * zi + cr;
                    // im = fabs(zr * zi) * 2.0 + ci;
                    re = _mm256_add_pd(_mm256_sub_pd(zr2, zi2), cr_vec);

                    __m256d im_intermediate = _mm256_mul_pd(zr,zi);
                    // https://stackoverflow.com/questions/63599391/find-absolute-in-avx#:~:text=So%20absolute%20value%20can%20be,mask%20for%20the%20sign%20bit.
                    // __m256 sign_bit = _mm256_set1_ps(-0.0f);
                    // something not working here, try this
                    // const __m256d sign_bit = _mm256_castsi256_pd(_mm256_set1_epi64x(0x8000000000000000));
                    __m256d abs = _mm256_andnot_pd(sign_bit, im_intermediate);
                    __m256d im_intermediate_2 = _mm256_mul_pd(abs,_two);
                    im = _mm256_add_pd(im_intermediate_2, ci_vec);

                    // ^ this should have worked. need to check.

                    zr = re;
                    zi = im;
                }
                // ^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

                // ********************* Grayscale value converter
                // when you have done the maximum number of iter for the 4 pixels OR if all 4 pixels are simply greater than 4
                // use the result to convert to grayscale value




                // SKIP THIS FOR NOW , MAY NOT AFFECT RESULT
                // make a mask to compare each of the four values to the max_iter
                // __m256i max_minus_value = _mm256_sub_epi32(max_vec, counter_vec);
                // ^ this should return num iter taken ex: [499,10,1,213]

                // try with _CMP_NEQ_UQ predicate
                // __m256d mask_grayscale_is_zero = _mm256_cmp_pd(max_minus_value, _zero, _CMP_NEQ_UQ);
                 // ^ this should provide ex. [0,1,1,0] for (false, true, true, false)
                 // ^ for this one, it produces 0 (false) for where max-value != 0

                // we want the grayscale value should be 0, when max_iter - value == 0
                // if max_minus_value ==0, then grayscale value should be 0.
                // so, using NEQ will provide us 0 
                // END OF SKIP



                // ***
                // do the valculation
                // int grayscale_val = round(sqrt (value / (float) maxIter) * 255);
                // 

                // make values_1 a double
                // but you have to cast each vector to double first
                // cast 32 bit int to 32 bit pd _mm256_cvtepi32_pd
                // should be fine to use single precision for now since idk how to do it for double
                // may need to check the result for mixing single precision and double
                // not sure how the register maps them , if it does, correctly
                //  __m256 values_1 = _mm256_div_ps(_mm256_cvtepi32_ps(counter_vec), _mm256_cvtepi32_ps(max_vec));


                // division can only be done in single precision i guess ?
                __m256 counter_ps = _mm256_cvtepi32_ps(counter_vec);
                __m256 max_ps = _mm256_cvtepi32_ps(max_vec);
                __m256 result_ps = _mm256_div_ps(counter_ps, max_ps);

                // convert it back to a packed INT
                //__m256i result_int = _mm256_cvtps_epi32(result_ps);

                __m256 values_ps = _mm256_sqrt_ps(result_ps);
                __m256 values_2 = _mm256_mul_ps(values_ps, _twofivefive);

                // https://stackoverflow.com/questions/61461613/my-sse-avx-optimization-for-element-wise-sqrt-is-no-boosting-why
                // values_1 = _mm256_sqrt_ps(values_1);
                //__m256 values_2 = _mm256_mul_ps(values_1, _twofivefive);

                // then i need to round this to the nearest integer

                // https://www.intel.com/content/www/us/en/docs/intrinsics-guide/index.html#text=_mm256_round_pd&ig_expand=6159
                __m256 grayscale_ps = _mm256_round_ps(values_2, _MM_FROUND_TO_NEAREST_INT |_MM_FROUND_NO_EXC);
                

                // convert it back to a packed INT
                __m256i grayscale_int = _mm256_cvtps_epi32(grayscale_ps);

                
                // __m256i grayscales_int = _mm256_cvtps_epi32(grayscale_values);

                // finally, multiply the multiply grayscale by the values
                //__m256d values = _mm256_mul_pd(mask_grayscale_is_zero, grayscales_int);
                // ***


                // ok now that I have my values, I can store them back in the pixel matrix
                int index = i+j*rows;
                // _mm256_store_si256((__m256i*)&pixelMatrix[index], grayscale_int);

                pixelMatrix[index + 0] = int(grayscale_int[3]);
                pixelMatrix[index + 1] = int(grayscale_int[2]);
                pixelMatrix[index + 2] = int(grayscale_int[1]);
                pixelMatrix[index + 3] = int(grayscale_int[0]);

                // counter intuitive, because this would normally be i*rows + j
                // but the true fractal is actually upside down, so we flip it
                // and make it i+rows*j
                // pixelMatrix[i+j*rows] = grayscale_value;
            }

            if (i%200 == 0){
                printf("row %d/%d \n", i, rows);
            }
        }
    }


    void write_pixels_to_image_file(Mat &img, int*pixelMatrix, int rows, int cols) {
        // uses openCV Mat datatype to write the pixel values and save image to disk
        for (int i = 0; i < rows; i++)
        {
            for (int j = 0; j < cols; j++)
            {
                int grayscale_int = pixelMatrix[i+j*rows];
                uchar value = (uchar) grayscale_int;
                img.ptr<uchar>(i)[j] = value;

                // printf("%d ", value);
            }
        }    
    }
}



int main()
{
    // define the image dimensions
    int rows_x = 9600;
    int cols_y = 10800;

    Timer t;
    t.tic();
    // allocate memory to be used for storing pixel valuess
    int* pixelMatrix = (int*) aligned_alloc(32, rows_x * cols_y * sizeof(int));
    memset(pixelMatrix, 0, rows_x * cols_y * sizeof(int));
    printf("time to aligned malloc = %f s\n", t.toc());


    // define the bounds of the burningship fractal domain 
    float x1 = -2.2f, x2 = 2.2f;
    float y1 = -2.2f, y2 = 2.2f;

    // used for mapping the pixels to the domain
    float scaleX = cols_y / (x2 - x1); // ->  9600 / (2.2 - -2.2) ~= 2000
    float scaleY = rows_x / (y2 - y1); // ->  10800 / (2.2 - -2.2) ~= 2000


    //! [color the set of pixels in the set vs not in the set]
    t.tic();
    simdburningship(pixelMatrix, rows_x, cols_y, x1, y1, scaleX, scaleY);
    printf("time to compute SIMD version = %f s\n", t.toc());


    // Render results to image file with openCV
    Mat burningshipImg(rows_x, cols_y, CV_8U);
    write_pixels_to_image_file(burningshipImg, pixelMatrix, rows_x, cols_y);
    imwrite("burningship_simd.png", burningshipImg);

    free(pixelMatrix);

    return EXIT_SUCCESS;
}























