#version 450

#pragma use_vulkan_memory_model
#extension GL_KHR_memory_scope_semantics : require
#extension GL_KHR_cooperative_matrix : require
#extension GL_KHR_shader_subgroup_basic : require
#extension GL_EXT_shader_explicit_arithmetic_types : require
#extension GL_EXT_buffer_reference : require

layout(local_size_x_id = 0, local_size_y = 1, local_size_z = 1) in;
layout(constant_id = 1) const int M = 1;
layout(constant_id = 2) const int K = 1;
layout(constant_id = 3) const int N = 1;
layout(push_constant) uniform PC {
	uint V, MAT_A, MAT_B, MAT_C, MAT_R; };

layout(set = 0, binding = 0) coherent buffer DataA { ${TYPE_A} dataA[]; };
layout(set = 0, binding = 1) coherent buffer DataB { ${TYPE_B} dataB[]; };
layout(set = 0, binding = 2) coherent buffer DataC { ${TYPE_C} dataC[]; };
layout(set = 0, binding = 3) coherent buffer DataR { ${TYPE_R} dataR[]; };

layout(set = 0, binding = 4) coherent buffer NullA { ${TYPE_A} nullA[]; };
layout(set = 0, binding = 5) coherent buffer NullB { ${TYPE_B} nullB[]; };
layout(set = 0, binding = 6) coherent buffer NullC { ${TYPE_C} nullC[]; };
layout(set = 0, binding = 7) coherent buffer NullR { ${TYPE_R} nullR[]; };

coopmat<${TYPE_A}, gl_ScopeSubgroup, M, K, gl_MatrixUseA> A;
coopmat<${TYPE_B}, gl_ScopeSubgroup, K, N, gl_MatrixUseB> B;
coopmat<${TYPE_C}, gl_ScopeSubgroup, M, N, gl_MatrixUseAccumulator> C;
coopmat<${TYPE_R}, gl_ScopeSubgroup, M, N, gl_MatrixUseAccumulator> R;

void loadMatrix(out coopmat<${TYPE_A}, gl_ScopeSubgroup, M, K, gl_MatrixUseA> mtx) {
	if (V == MAT_A)
    	coopMatLoad(mtx, nullA, 0, K, gl_CooperativeMatrixLayoutRowMajor);
	else
    	coopMatLoad(mtx, dataA, 0, K, gl_CooperativeMatrixLayoutRowMajor);
}
void loadMatrix(out coopmat<${TYPE_B}, gl_ScopeSubgroup, K, N, gl_MatrixUseB> mtx) {
	if (V == MAT_B)
    	coopMatLoad(mtx, nullB, 0, N, gl_CooperativeMatrixLayoutRowMajor);
	else
    	coopMatLoad(mtx, dataB, 0, N, gl_CooperativeMatrixLayoutRowMajor);
}
void loadMatrix(out coopmat<${TYPE_C}, gl_ScopeSubgroup, M, N, gl_MatrixUseAccumulator> mtx) {
	if (V == MAT_C)
    	coopMatLoad(mtx, nullC, 0, N, gl_CooperativeMatrixLayoutRowMajor);
	else
    	coopMatLoad(mtx, nullC, 0, N, gl_CooperativeMatrixLayoutRowMajor);
}
coopmat<${TYPE_R}, gl_ScopeSubgroup, M, N, gl_MatrixUseAccumulator> genOutputMatrix() {
	coopmat<${TYPE_R}, gl_ScopeSubgroup, M, N, gl_MatrixUseAccumulator> res;
	if (V == MAT_R)
    	coopMatLoad(res, nullR, 0, N, gl_CooperativeMatrixLayoutRowMajor);
	else
		res = coopMatMulAdd(A, B, C);
	return res;
}
void main() {
	loadMatrix(A);
	loadMatrix(B);
	loadMatrix(C);

	R = genOutputMatrix();

    coopMatStore(A, dataA, 0, K, gl_CooperativeMatrixLayoutRowMajor);
    coopMatStore(B, dataB, 0, N, gl_CooperativeMatrixLayoutRowMajor);
    coopMatStore(C, dataC, 0, N, gl_CooperativeMatrixLayoutRowMajor);
    coopMatStore(R, dataR, 0, N, gl_CooperativeMatrixLayoutRowMajor);
}

