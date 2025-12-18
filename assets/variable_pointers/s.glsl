#version 450

#extension GL_EXT_buffer_reference : require
#extension GL_EXT_scalar_block_layout : require
#extension GL_EXT_shader_explicit_arithmetic_types : require

layout(local_size_x_id = 0, local_size_y_id = 1, local_size_z_id = 2) in;

layout(push_constant) uniform PC {
	uint x,y,z,w;
} pc;

struct _uvec3 { uint x, y, z; };
_uvec3 muvec3(uint x, uint y, uint z) {
	_uvec3 res;
	res.x = x;
	res.y = y;
	res.z = z;
	return res;
}
layout(std430, buffer_reference) buffer FooStruct {
	uint64_t addr;
	uvec4 data[];
};
layout(std430, buffer_reference) buffer BarStruct {
	uint64_t addr;
	uint64_t reserved; // both std430 and std140 add a pad after addr field
	_uvec3 data[];     // change from uvec3 to packed_uvec3 to enforce stride 12
};
layout(binding = 0) buffer FooBuffer { uint64_t addr; } fooBuffer;
layout(binding = 1) buffer BarBuffer { uint64_t addr; } barBuffer;
layout(binding = 2) buffer AuxBuffer { uint data[]; } auxBuffer;

void main()
{
	FooStruct foo = pc.x == 0 ? FooStruct(fooBuffer.addr) : FooStruct(barBuffer.addr);
	BarStruct bar = pc.x == 0 ? BarStruct(barBuffer.addr) : BarStruct(fooBuffer.addr);
	const uint sum = pc.x + pc.y + pc.z + pc.w;
	foo.data[pc.y] = uvec4(sum, pc.y, pc.z, pc.w);
	bar.data[pc.y] = muvec3(sum, pc.y, pc.z);

	auxBuffer.data[pc.x == 0 ? 0 : 1] = pc.y;
}


