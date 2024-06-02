# DXR infinity mirror

Just open the solution in visual studion 22 and you should be able to build it. There are some settings you can play around with in settings.h. 

The benchmark will run on whatever resolution is set in settings and then test every level of max recursion depth. settings can be used to configure how many frames are used for each test to calculate the frame time average.

Frame times are measured between presents in the DirectLoop in DX12Base.cpp.

After all tests are complete the application will output the results into a csv file and terminate itself.