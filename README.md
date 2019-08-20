To render you must compile, tested on linux, not tested on windows yet.\
Things you can saily change:\
-xmid, ymid: real and imag centre of the image\
-size: zoom - higher means zoomed out\
-width, height: dimensions of the output file\
-iterations: number of attempts of computing buddhabrot orbits (not the same as iterations of divergence, like in classic mandelbrot)\
-func: complex function of the fractal (classic mandelbrot/nebulabrot: z = z * z + c;)

Things that can also be done but not so easily:\
-img_func, img_monochrome: functions computing the values of pixels based on fractal results\
-manager.add(...); : how many separate images/fractals are generated to get the result, also the first argument is iterations of divergence\
-img_manager.add(...); : how many images are saved and using what image function\
-collection.loadFile, collection.saveFile: save iteration results in raw numbers (files are big, like 200 MiB), so they can be loaded later and rendered with other image options or merged with more iteration results\
-func_whole: function that computes the whole image, not individual pixels, should be put in the img_manager.add("iall", ImageOutputData(ImageFunctionData(...), ...));\
-dynamic function loading, compilation of function before rendering, definitely linux exclisive: bunch of commented code in main.cpp (uncomment #target_link_libraries(nebulabrotgen dl))\
\
To run (linux):\
cmake -DCMAKE_BUILD_TYPE=Release\
make\
./nebulabrotgen\
\
Examples:\
https://drive.google.com/open?id=1NGYRuzK0Bpp1TY1gfyXnizgmUKXHnGaf
