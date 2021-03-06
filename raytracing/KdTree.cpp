#include "KdTree.h"

KdTreeNode::KdTreeNode(){
	left = NULL;
	right = NULL;
	parent = NULL;
	depth = 0;
	lowerBound = Vector3D(0,0,0);
	upperBound = Vector3D(0,0,0);
	vid = -1;
}

KdTreeNode::KdTreeNode(KdTreeNode* parent, UINT depth, Vector3D lb,
					   Vector3D ub){
	this->parent = parent;
	this->depth = depth;
	this->lowerBound = lb;
	this->upperBound = ub;
	this->left = NULL;
	this->right = NULL;
	this->vid = -1;
}

KdTreeNode::~KdTreeNode(){
	delete	left;
	delete	right;
}

bool KdTreeNode::enclose(Vector3D point){
	if(upperBound.x + DOUBLE_EPS > point.x && 
	   lowerBound.x - DOUBLE_EPS < point.x && 
	   upperBound.y + DOUBLE_EPS > point.y && 
	   lowerBound.y - DOUBLE_EPS < point.y && 
	   upperBound.z + DOUBLE_EPS > point.z && 
	   lowerBound.z - DOUBLE_EPS < point.z){
		return true;
	} else { 
		return false;
	}
	return false;
}

KdTree::KdTree(){
	this->root = new KdTreeNode(NULL, 0, 
		-INFTY * Vector3D(1,1,1), INFTY * Vector3D(1,1,1));
	this->mesh = NULL;
	this->radius = 0;
}

KdTree::KdTree(CMesh* mesh){
	this->mesh = mesh;
	constructFromMesh();
}

KdTree::~KdTree(){
	delete root;
	for (int i = 0; i < MAX_THREAD; ++i) {
		delete[] face_count[i];
	}
}

void KdTree::constructFromMesh(){
	Vector3D lowerBound =  INFTY * Vector3D(1,1,1);
	Vector3D upperBound = -INFTY * Vector3D(1,1,1);
	radius = 0;

	for (UINT i = 0; i < mesh->m_nVertex; ++i) {
		Vector3D p = mesh->m_pVertex[i].m_vPosition;
		if (p.x < lowerBound.x) lowerBound.x = p.x;
		if (p.y < lowerBound.y) lowerBound.y = p.y;
		if (p.z < lowerBound.z) lowerBound.z = p.z;
		if (p.x > upperBound.x) upperBound.x = p.x;
		if (p.y > upperBound.y) upperBound.y = p.y;
		if (p.z > upperBound.z) upperBound.z = p.z;
	}
	for (UINT i = 0; i < mesh->m_nEdge; ++i) {
		UINT* vList = mesh->m_pEdge[i].m_iVertex;
		Vector3D v1 = mesh->m_pVertex[vList[0]].m_vPosition;
		Vector3D v2 = mesh->m_pVertex[vList[1]].m_vPosition;
		double d = (Vector3D(v1-v2)).length();
		if (d > radius) radius = d;
	}

	radius *= 1.01;
	for (int i = 0; i < MAX_THREAD; ++i) {
		face_count[i] = new unsigned char[mesh->m_nFace];
		memset(face_count[i], 0, sizeof(unsigned char)*mesh->m_nFace);
	}

	this->root = new KdTreeNode(NULL, 0, lowerBound, upperBound);

	for (UINT i = 0; i < mesh->m_nVertex; ++i) {
		insert(i, root);
	}
}

void KdTree::insert(UINT vid, KdTreeNode* node){
	Vector3D point = mesh->m_pVertex[vid].m_vPosition;
	if (!node->enclose(point)) return;
	if (node->vid == -1) {
		if (node->left == NULL) {
			node->vid = vid;
		} else {
			if (node->left->enclose(point)) {
				insert(vid, node->left);
			} else {
				insert(vid, node->right);
			}
		}
	} else {
		Vector3D newUB = node->upperBound;
		Vector3D newLB = node->lowerBound;
		switch(node->depth % 3){
			case 0:
				newUB.x = (node->upperBound.x+node->lowerBound.x)/2;
				newLB.x = (node->upperBound.x+node->lowerBound.x)/2;
				break;
			case 1:
				newUB.y = (node->upperBound.y+node->lowerBound.y)/2;
				newLB.y = (node->upperBound.y+node->lowerBound.y)/2;
				break;
			case 2:
				newUB.z = (node->upperBound.z+node->lowerBound.z)/2;
				newLB.z = (node->upperBound.z+node->lowerBound.z)/2;
				break;
			default:
				break;
		}
		node->left = new KdTreeNode(node, node->depth+1, 
								node->lowerBound, newUB);
		node->right = new KdTreeNode(node, node->depth+1,
								newLB, node->upperBound);
		Vector3D oPoint = mesh->m_pVertex[node->vid].m_vPosition;
		if (node->left->enclose(oPoint)) {
			node->left->vid = node->vid;
		} else {
			node->right->vid = node->vid;
		}
		node->vid = -1;
		
		insert(vid, node);	
	}
}

void KdTree::getFaceList(vector<UINT> &vid_vec, vector<UINT> &fid_vec){
	int id = omp_get_thread_num();

	for (UINT i = 0; i < vid_vec.size(); ++i){
		for (UINT j = 0; j < (UINT)mesh->m_pVertex[vid_vec[i]].m_nValence; ++j){
			UINT e = mesh->m_pVertex[vid_vec[i]].m_piEdge[j];
			UINT f = mesh->m_pEdge[e].m_iFace;
			
			face_count[id][f] ++;
			if (face_count[id][f] == 3) {
				fid_vec.push_back(f);
			}
		}
	}
	for (UINT i = 0; i < vid_vec.size(); ++i){
		for (UINT j = 0; j < (UINT)mesh->m_pVertex[vid_vec[i]].m_nValence; ++j){
			UINT e = mesh->m_pVertex[vid_vec[i]].m_piEdge[j];
			UINT f = mesh->m_pEdge[e].m_iFace;
			face_count[id][f] = 0;
		}
	}
}

void KdTree::searchVertexInBox(Vector3D lower, Vector3D upper, vector<UINT> &vid_vec){
	search(lower,upper,root, vid_vec);
}

void KdTree::searchFaceInBox(Vector3D lower, Vector3D upper, vector<UINT> &fid_vec){
	vector<UINT> vid_vec(0);
	search(lower,upper,root,vid_vec);
	getFaceList(vid_vec, fid_vec);
}

void KdTree::search(Vector3D lower, Vector3D upper, KdTreeNode* node, vector<UINT> &vec){
	if (node->vid != -1){	
		Vector3D point = mesh->m_pVertex[node->vid].m_vPosition;
		if( point.x+DOUBLE_EPS > lower.x && 
			point.x-DOUBLE_EPS < upper.x &&
			point.y+DOUBLE_EPS > lower.y &&
			point.y-DOUBLE_EPS < upper.y &&
			point.z+DOUBLE_EPS > lower.z &&
			point.z+DOUBLE_EPS < upper.z){
				vec.push_back(node->vid);
		}
	}
	if(node->upperBound.x < lower.x) return;
	if(node->upperBound.y < lower.y) return;
	if(node->upperBound.z < lower.z) return;
	if(node->lowerBound.x > upper.x) return;
	if(node->lowerBound.y > upper.y) return;
	if(node->lowerBound.z > upper.z) return;	
	if(node->left != NULL) {
		search(lower, upper, node->left, vec);
	}
	if(node->right != NULL) {
		search(lower, upper, node->right, vec);
	}
}

bool KdTree::hasPoints(Vector3D lower, Vector3D upper, KdTreeNode* node){
	if (node->left == NULL) {
		if (node->vid == -1){
			return false;
		} else {	
			Vector3D point = mesh->m_pVertex[node->vid].m_vPosition;
			if (lower.x - DOUBLE_EPS < point.x &&
				lower.y - DOUBLE_EPS < point.y &&
				lower.z - DOUBLE_EPS < point.z &&
				upper.x + DOUBLE_EPS > point.x &&
				upper.y + DOUBLE_EPS > point.y &&
				upper.z + DOUBLE_EPS > point.z) {
				return true;
			}
		}
	} else {
		if (lower.x - DOUBLE_EPS < node->lowerBound.x &&	
			lower.y - DOUBLE_EPS < node->lowerBound.y &&
			lower.z - DOUBLE_EPS < node->lowerBound.z &&
			upper.x + DOUBLE_EPS > node->upperBound.x &&
			upper.y + DOUBLE_EPS > node->upperBound.y &&
			upper.z + DOUBLE_EPS > node->upperBound.z) {
			return true;
		}
		if(node->upperBound.x < lower.x) return false;
		if(node->upperBound.y < lower.y) return false;
		if(node->upperBound.z < lower.z) return false;
		if(node->lowerBound.x > upper.x) return false;
		if(node->lowerBound.y > upper.y) return false;
		if(node->lowerBound.z > upper.z) return false;	
		if (hasPoints(lower, upper, node->left)) return true;
		if (hasPoints(lower, upper, node->right)) return true;
		return false;
	}
	return false;
}

void KdTree::searchLine_int(Vector3D v1, Vector3D v2, vector<UINT> &vid_vec){
	Vector3D lb(min(v1.x,v2.x), min(v1.y,v2.y), min(v1.z,v2.z));
	Vector3D ub = v1 + v2 - lb;
	lb -= radius * Vector3D(1,1,1);
	ub += radius * Vector3D(1,1,1);
	if (Vector3D(v2-v1).normalize() < 4*radius) {
		searchVertexInBox(lb, ub, vid_vec);
	} else if (hasPoints(lb, ub, root)) {
		searchLine_int(v1, (v1+v2)/2, vid_vec);
		searchLine_int((v1+v2)/2, v2, vid_vec);
	}
	
}

inline void KdTree::searchLine_fast_int(Vector3D o, Vector3D d, double s, double t, 
								 KdTreeNode* node, vector<UINT> &vid_vec) {
	if (node->left == NULL) {
		if (node->vid == -1) {
			return;
		} else {
			Vector3D point = mesh->m_pVertex[node->vid].m_vPosition;
			Vector3D distVec = ((point - o) * d) * d - point + o;
			double dist = distVec.length2();
			if (dist < this->radius * this->radius + DOUBLE_EPS) {
				vid_vec.push_back(node->vid);
			}
		}
	} else {
		double mint = 1e9;
		double maxt = -1e9;
		Vector3D lower = node->lowerBound - radius * Vector3D(1, 1, 1) - o;
		Vector3D upper = node->upperBound + radius * Vector3D(1, 1, 1) - o;

		double dt[6] = {1e9, 1e9, 1e9, 1e9, 1e9, 1e9};
		if (fabs(d.x) > DOUBLE_EPS) {
			dt[0] = lower.x / d.x;
			dt[3] = upper.x / d.x;
		}
		if (fabs(d.y) > DOUBLE_EPS) {	
			dt[1] = lower.y / d.y;
			dt[4] = upper.y / d.y;
		}
		if (fabs(d.z) > DOUBLE_EPS) {
			dt[2] = lower.z / d.z;
			dt[5] = upper.z / d.z;
		}

		bool intersect = false;
		for (int i = 0; i < 6; ++i) {
			if (dt[i] > mint && dt[i] < maxt) {
				continue;
			}
			Vector3D tp = dt[i] * d;
			if (upper.x + DOUBLE_EPS > tp.x && 
				lower.x - DOUBLE_EPS < tp.x && 
				upper.y + DOUBLE_EPS > tp.y && 
				lower.y - DOUBLE_EPS < tp.y && 
				upper.z + DOUBLE_EPS > tp.z && 
				lower.z - DOUBLE_EPS < tp.z) {
				intersect = true;
				if (dt[i] < mint) {
					mint = dt[i];
				}
				if (dt[i] > maxt) {
					maxt = dt[i];
				}
			}
		}
		if (intersect && mint < t + DOUBLE_EPS && maxt > s - DOUBLE_EPS) {
			searchLine_fast_int(o, d, max(s, mint), min(t, maxt), node->left, vid_vec);
			searchLine_fast_int(o, d, max(s, mint), min(t, maxt), node->right, vid_vec);
		}
	}
}

void KdTree::searchLine(Vector3D origin, Vector3D dir, double s, double t, vector<UINT> &fid_vec){
	vector<UINT> vid_vec(0);
	//int start = clock();
	searchLine_fast_int(origin, dir, s, t, root, vid_vec);
	//int mid = clock();
	getFaceList(vid_vec, fid_vec);
	//int end = clock();
	//searchTime += (mid - start);
	//faceTime += (end - mid);
	//++ callTime;
	//if (callTime % 100000 == 0) {
	//	cout << searchTime << " " << faceTime << " " << callTime << endl;
	//}
}