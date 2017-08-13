struct dawg_node {
	bool eow;
	struct dawg_node* next;
	struct dawg_node* child;
};

struct scan {
	struct dawg_node* target;
	struct scan* next;
	struct scan* child;
};

/* strategy:
	 have two graphs here, scan and dawg


	 D O G
	 L O G O
	 L O O S E
	 T O

	 ->
	 D O G*
	 L O G O*
	     v
	     O O S E*
	 T O*
	 ->
	 D O G*
	 z
	 
	 T v
	 D O* G* O
	 L ^
	 
	 to add a node,
	 - take the first letter
	 - compare with the head of scan, and either add next, or take that one.

