# DXR infinity mirror

Just open the solution in visual studio 22 and you should be able to build it. There are some settings you can play around with in settings.h. 

The setup used for automating benchmarking for the frame times is in the git branch "Benchmarking".

## Overview
This was done as my project for a course in DirectX12 that I had at university. I decided to work with raytracing and learning how to utilize raytracing-acceleration cores.

I was curious to see what may be the limits of how many reflections you can have in a scene and if it possible to recreate so called infinity mirror effects in rendering.

![alt text](Presentation/no%20reflect.gif)

For my test I used a rhombic dodecahedron, the object seen in the gif above, with the outer faces inverted. These faces being inverted meant that rays can enter the object volume but not escape. In the above gif there are no reflections taking place.

It turns out that there is a hardware set limit on how many ray sub-dispatches you can have i.e. reflections in this case. That limit is 31 including the first ray you cast. So, the maximum number of reflections you can have is 30, showcased below.

![alt text](Presentation/flat.gif)

The edges are made darker for each bounce, giving a realistic infinity mirror effect where each reflection doesn’t perfectly reflect all the light. The rhombic dodecahedron was chosen as it was expected to create a cool effect, and I knew that this shape could fill space with no gaps and thus it would be easier to see if the resulting lattice is correct or if my implementation introduced distortions.

I also tried making all the reflective faces smooth shaded, which could create some truly mind-bending effects as seen below.

![alt text](Presentation/smooth.gif)

These effects are neat however probably don't have much practical use in most games. I do have some ideas for interesting abstract games that could utilize this effect for some mind-blowing imagery.

I believe it would be possible to surpass the 30-bounce limit as well. By saving the current positions and directions of each remaining ray in buffers, a completely new ray rendering pass could be utilized for yet another 30 bounces. This would likely be rather costly and impractical. But likely very possible!
