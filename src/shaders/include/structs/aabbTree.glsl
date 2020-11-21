struct AabbTreeNode {
	vec4 leftAabbMin;
	vec4 leftAabbMax;
	vec4 rightAabbMin;
	vec4 rightAabbMax;
	int leftChild;
	int rightChild;
};
struct Triangle {
	vec4 p1;
	vec4 p2;
	vec4 p3;
};
