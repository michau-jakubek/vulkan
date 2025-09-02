#version 450

#extension GL_EXT_buffer_reference : require
#extension GL_EXT_shader_explicit_arithmetic_types : require

layout(binding = 0) buffer FooBuffer { uvec4 fooData[]; };

layout(buffer_reference) buffer FooBufferRef { uvec4 fooData[]; };
layout(binding = 2) buffer RefsBuffer { FooBufferRef fooBufferRef; } refs;

layout(push_constant) uniform PC {
	uint x,y,z,w;
	FooBufferRef fooBufferRef;
	uint64_t fooBufferRef2;
} pc;
layout(local_size_x_id = 0, local_size_y_id = 1, local_size_z_id = 2) in;

void main()
{
	if (pc.x == 0)
		fooData[0].x = 315018;

	FooBufferRef fooBufferRef = refs.fooBufferRef;
	fooBufferRef.fooData[1].x = 123;

	FooBufferRef fooBufferRef2 = FooBufferRef(pc.fooBufferRef);
	fooBufferRef2.fooData[1].y = 513;

	FooBufferRef fooBufferRef3 = FooBufferRef(pc.fooBufferRef2);
	fooBufferRef3.fooData[1].z = 911;

	fooBufferRef.fooData[2].x = gl_WorkGroupSize.x;
	fooBufferRef.fooData[2].y = gl_WorkGroupSize.y;
	fooBufferRef.fooData[2].z = gl_WorkGroupSize.z;
}

