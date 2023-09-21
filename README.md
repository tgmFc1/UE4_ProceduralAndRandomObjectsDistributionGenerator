# Procedural and random objects distribution generator plugin for Unreal Engine 4
![Generation process basic algorithm - ](/Helpers/Images/GenerationBaseMethod.png)
## Link to download compiled version - https://drive.google.com/file/d/1oKJxO1bQQiJp7lV-29jYkuHwkZQbhyNh/view?usp=drive_link
## Link to documentation - https://drive.google.com/file/d/1BAg9BXK0jFfdDS1_Xk9CuigE8RvquTyN/view?usp=drive_link
## Link to tutorial - https://youtu.be/722mMo0YEDE
How this plugin works: this plugin adds new object and actor classes into engine for create defined objects(actors or actors components) 
at defined level with procedurally generated 3D space transformation based on user defined options and parameters. New “ProcGenActor” actor 
class mostly responsible for generation logic (on spline shape/selected landscape) and new “PGSObj”(Procedural Generation Slot Object) mostly 
responsible for generation parameters and have internal generation logic. “ProcGenActor” is always used with “PGSObj” object except situations 
when “ProcGenActor” used as forbidden shape for other “ProcGenActor” actors. 
### What’s new in latest 0.046 plugin version: 
### + Generation process are exposed to blueprint so users can create their custom generation blueprints inside “PGSObj”’s.
### + Generation grid feature, used as an alternative of in engine “Grass Maps” and for distance detection checks optimizations in generation process.(at this moment still in development and not production ready)
### + Generation parameters modifiers actors in defined areas.(at now can be used to modify scale or generation coefficient, also support curves)
### + Optional complex surface slope and surface edges detecting in generation process.
### + Distance from “ProcGenActor” spline shape edges and from excluded shapes edges based generation options.(also support curves)
### + Generation progress bars in editor.(at this moment with minimum of information)
### P.S Please read documentation before using this plugin.
