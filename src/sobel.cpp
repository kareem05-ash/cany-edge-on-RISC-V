#include "sobel.h"
#include <cstdint>
#include <cstdlib>
                                        
void sobel (const Image& src, int16_t* Gx, int16_t* Gy){
    
    static const int16_t Kx[3][3] = {                                               // Sobel X Kernal: detects vertical edges
        {-1, 0, 1},
        {-2, 0, 2},
        {-1, 0, 1}
    };
    
    static const int16_t Ky[3][3] = {                                               // Sobel Y Kernal: detects horizontal edges 
        {-1, -2, -1},
        {0,  0,  0},
        {1,  2,  1}
    };
    const int R=Sob_Rad; 

    for(int y=0; y<src.height; y++){
        for(int x=0; x<src.width; x++){
            
            int16_t gx = 0, gy = 0;
    
                for(int ky=-R; ky<=R; ky++){
                    for(int kx=-R; kx<=R; kx++){
                        
                        int sy = y + ky;                                            // Slide 3*3 Matrix
                        int sx = x + kx;
    
                        uint8_t pixel = 0;                                          // Zero_Padding
                        if (sy>= 0 && sy<src.height  && sx>= 0 && sx<src.width){
                            
                            pixel = src.data[sy *src.width + sx];                   
                        }
    
    
                            gx += pixel * Kx[ky+R][kx+R];                           // Calculate gx on all simple matrix
                            gy += pixel * Ky[ky+R][kx+R];                           // Calculate gy on all simple matrix
                    }                   
                }
         Gx[y *src.width + x] = gx;                                                 // Store Gx in SOA
         Gy[y *src.width + x] = gy;                                                 // Store Gy in SOA
        }  
    }
}



    