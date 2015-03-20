# path-tracer-nacl
This is a Native Client version of [path-tracer](https://github.com/sdao/path-tracer); the code is adapted from the version at Git commit `0b51286`.

It's a bit stripped down. Mesh import has been removed (it required an external library), so only primitives can be rendered. Acceleration via Intel Embree has been removed (it too required an external library), so intersection testing occurs in linear time (decent when you only have primitives!).

Right now, the scene to be rendered is hard-coded in (although the scene parsing mechanism is still present).

Check out a live demo [here](https://sdao.github.io/path-tracer-nacl)!
