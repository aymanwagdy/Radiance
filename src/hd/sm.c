/* Copyright (c) 1998 Silicon Graphics, Inc. */

#ifndef lint
static char SCCSid[] = "$SunId$ SGI";
#endif

/*
 *  sm.c
 */
#include "standard.h"
#include "sm_list.h"
#include "sm_geom.h"
#include "sm.h"


SM *smMesh = NULL;
double smDist_sum=0;
int smNew_tri_cnt=0;

static int smBase_nbrs[4][3] =  { {3,2,1},{3,0,2},{3,1,0},{1,2,0}};

#ifdef TEST_DRIVER
VIEW  Current_View = {0,{0,0,0},{0,0,-1},{0,1,0},60,60,0};
int Pick_cnt =0;
int Pick_tri = -1,Picking = FALSE,Pick_samp=-1;
FVECT Pick_point[500],Pick_origin,Pick_dir;
FVECT  Pick_v0[500], Pick_v1[500], Pick_v2[500];
FVECT P0,P1,P2;
FVECT FrustumNear[4],FrustumFar[4];
double dev_zmin=.01,dev_zmax=1000;
#endif

smDir(sm,ps,id)
   SM *sm;
   FVECT ps;
   int id;
{
    FVECT p;
    
    VCOPY(p,SM_NTH_WV(sm,id));
    point_on_sphere(ps,p,SM_VIEW_CENTER(sm)); 
}
smDir_in_cone(sm,ps,id)
   SM *sm;
   FVECT ps;
   int id;
{
    FVECT p;
    
    VCOPY(p,SM_NTH_WV(sm,id));
    point_on_sphere(ps,p,SM_VIEW_CENTER(sm)); 
}

smClear_mesh(sm)
    SM *sm;
{
    /* Reset the triangle counters */
    SM_TRI_CNT(sm) = 0;
    SM_NUM_TRIS(sm) = 0;
    SM_FREE_TRIS(sm) = -1;
}

smClear_flags(sm,which)
SM *sm;
int which;
{
  int i;

  if(which== -1)
    for(i=0; i < T_FLAGS;i++)
      bzero(SM_NTH_FLAGS(sm,i),T_TOTAL_FLAG_BYTES(SM_MAX_TRIS(sm)));
  else
    bzero(SM_NTH_FLAGS(sm,which),T_TOTAL_FLAG_BYTES(SM_MAX_TRIS(sm)));
}

smClear_locator(sm)
SM *sm;
{
  STREE  *st;
  
  st = SM_LOCATOR(sm);
 
  stClear(st);
}

smInit_locator(sm,center,base)
SM *sm;
FVECT  center,base[4];
{
  STREE  *st;
  
  st = SM_LOCATOR(sm);
 
  stInit(st,center,base);

}

smClear(sm)
SM *sm;
{
  smClear_samples(sm);
  smClear_mesh(sm);
  smClear_locator(sm);
}

int
smAlloc_tris(sm,max_verts,max_tris)
SM *sm;
int max_verts,max_tris;
{
    int i,nbytes,vbytes,fbytes;

    vbytes = max_verts*sizeof(VERT);
    fbytes = T_TOTAL_FLAG_BYTES(max_tris);
    nbytes = vbytes + max_tris*sizeof(TRI) +T_FLAGS*fbytes + 8;
    for(i = 1024; nbytes > i; i <<= 1)
		;
    /* check if casting works correctly */
    max_tris = (i-vbytes-8)/(sizeof(TRI) + T_FLAG_BYTES);
    fbytes = T_TOTAL_FLAG_BYTES(max_tris);
    
    SM_BASE(sm)=(char *)malloc(vbytes+max_tris*sizeof(TRI)+T_FLAGS*fbytes);

    if (SM_BASE(sm) == NULL)
       return(0);

    SM_TRIS(sm) = (TRI *)SM_BASE(sm);
    SM_VERTS(sm) = (VERT *)(SM_TRIS(sm) + max_tris);

    SM_NTH_FLAGS(sm,0) = (int4 *)(SM_VERTS(sm) + max_verts);
    for(i=1; i < T_FLAGS; i++)
       SM_NTH_FLAGS(sm,i) = (int4 *)(SM_NTH_FLAGS(sm,i-1)+fbytes/sizeof(int4));
    
    SM_MAX_VERTS(sm) = max_verts;
    SM_MAX_TRIS(sm) = max_tris;

    smClear_mesh(sm);

    return(max_tris);
}



int
smAlloc_locator(sm)
SM *sm;
{
  STREE  *st;
  
  st = SM_LOCATOR(sm);
 
  st = stAlloc(st);
 
  if(st)
    return(TRUE);
  else
    return(FALSE);
}

/* Initialize/clear global smL sample list for at least n samples */
smAlloc(max_samples)
   register int max_samples;
{
  unsigned nbytes;
  register unsigned i;
  int total_points;
  int max_tris;

  /* If this is the first call, allocate sample,vertex and triangle lists */
  if(!smMesh)
  {
    if(!(smMesh = (SM *)malloc(sizeof(SM))))
       error(SYSTEM,"smAlloc():Unable to allocate memory");
    bzero(smMesh,sizeof(SM));
  }
  else
  {   /* If existing structure: first deallocate */
    if(SM_BASE(smMesh))
      free(SM_BASE(smMesh));
    if(SM_SAMP_BASE(smMesh))
       free(SM_SAMP_BASE(smMesh));
  }

  /* First allocate at least n samples + extra points:at least enough
   necessary to form the BASE MESH- Default = 4;
  */
  max_samples = smAlloc_samples(smMesh, max_samples,SM_EXTRA_POINTS);

  total_points = max_samples + SM_EXTRA_POINTS;
  max_tris = total_points*2;

  /* Now allocate space for mesh vertices and triangles */
  max_tris = smAlloc_tris(smMesh, total_points, max_tris);

  /* Initialize the structure for point,triangle location.
   */
  smAlloc_locator(smMesh);

}



smInit_mesh(sm,vp)
SM *sm;
FVECT vp;
{

  /* NOTE: Should be elsewhere?*/
  smDist_sum = 0;
  smNew_tri_cnt = 0;

  VCOPY(SM_VIEW_CENTER(smMesh),vp);
  smClear_locator(sm);
  smInit_locator(sm,vp,0);
  smClear_aux_samples(sm);
  smClear_mesh(sm);  
  smCreate_base_mesh(sm,SM_DEFAULT);
}

/*
 * int
 * smInit(n)		: Initialize/clear data structures for n entries
 * int	n;
 *
 * This routine allocates/initializes the sample, mesh, and point-location
 * structures for at least n samples.
 * If n is <= 0, then clear data structures.  Returns number samples
 * actually allocated.
 */

int
smInit(n)
   register int	n;
{
  int max_vertices;

  /* If n <=0, Just clear the existing structures */
  if(n <= 0)
  {
    smClear(smMesh);
    return(0);
  }

  /* Total mesh vertices includes the sample points and the extra vertices
     to form the base mesh
  */
  max_vertices = n + SM_EXTRA_POINTS;

  /* If the current mesh contains enough room, clear and return */
  if(smMesh && max_vertices <= SM_MAX_VERTS(smMesh))
  {
    smClear(smMesh);
    return(SM_MAX_SAMP(smMesh));
  }
  /* Otherwise- mesh must be allocated with the appropriate number of
    samples
  */
  smAlloc(n);

  return(SM_MAX_SAMP(smMesh));
}


smLocator_apply_func(sm,v0,v1,v2,edge_func,interior_func,arg1,arg2)
   SM *sm;
   FVECT v0,v1,v2;
   int (*edge_func)();
   int (*interior_func)();
   int *arg1,arg2;
{
  STREE *st;
  FVECT p0,p1,p2;

  st = SM_LOCATOR(sm);

  VSUB(p0,v0,SM_VIEW_CENTER(sm));
  VSUB(p1,v1,SM_VIEW_CENTER(sm));
  VSUB(p2,v2,SM_VIEW_CENTER(sm));

  stApply_to_tri(st,p0,p1,p2,edge_func,interior_func,arg1,arg2);

}


int
add_tri_expand(qtptr,q0,q1,q2,t0,t1,t2,n,arg,t_id,del_set)
QUADTREE *qtptr;
FVECT q0,q1,q2;
FVECT t0,t1,t2;
int n;
int *arg;
int t_id;
OBJECT *del_set;
{
    OBJECT t_set[QT_MAXSET+1],*optr,*tptr,r_set[QT_MAXSET+1];
    int i,id,found;
    FVECT v0,v1,v2;
    TRI *tri;
    
#ifdef DEBUG_TEST_DRIVER
    Pick_tri = t_id;
    Picking = TRUE;
#endif    
    
    if(QT_IS_EMPTY(*qtptr))
    {
      *qtptr = qtaddelem(*qtptr,t_id);
      return(TRUE);
    }

    optr = qtqueryset(*qtptr);
    if(del_set)
    {
	setintersect(r_set,del_set,optr);
	if(QT_SET_CNT(r_set) > 0)
	 {
	     qtgetset(t_set,*qtptr);
	     optr = QT_SET_PTR(r_set);
	     for(i = QT_SET_CNT(r_set); i > 0; i--)
	     {
		 id = QT_SET_NEXT_ELEM(optr);
		 deletelem(t_set,id);
	     }
	     qtfreeleaf(*qtptr);
	     *qtptr = qtnewleaf(t_set);
	     optr = t_set;
	 }
    }
    if(!inset(optr,t_id))
    {
      if(QT_SET_CNT(optr) < QT_MAXSET)
	*qtptr = qtaddelem(*qtptr,t_id);
      else
	{
#ifdef DEBUG
	  eputs("add_tri_expand():no room in set\n");
#endif
	  return(FALSE);
	}
    }
    optr = qtqueryset(*qtptr);
    if(QT_SET_CNT(optr) >= QT_SET_THRESHOLD)
      if (n < QT_MAX_LEVELS)
      {
	qtgetset(t_set,*qtptr);
	/* If set size exceeds threshold: subdivide cell and reinsert tris*/
	qtfreeleaf(*qtptr);
	qtSubdivide(qtptr);

	for(optr = QT_SET_PTR(t_set),i=QT_SET_CNT(t_set); i > 0; i--)
	{
	  id = QT_SET_NEXT_ELEM(optr);
	  tri = SM_NTH_TRI(smMesh,id);
	  VSUB(v0,SM_T_NTH_WV(smMesh,tri,0),SM_VIEW_CENTER(smMesh));
	  VSUB(v1,SM_T_NTH_WV(smMesh,tri,1),SM_VIEW_CENTER(smMesh));
	  VSUB(v2,SM_T_NTH_WV(smMesh,tri,2),SM_VIEW_CENTER(smMesh));
	  found = qtAdd_tri(qtptr,q0,q1,q2,v0,v1,v2,id,n);
#ifdef DEBUG
	  if(!found)
	    eputs("add_tri_expand():Reinsert\n");
#endif
	}
	return(QT_MODIFIED);
      }
      else
	if(QT_SET_CNT(optr) < QT_MAXSET)
	{
#ifdef DEBUG_TEST_DRIVER	      
	  eputs("add_tri_expand():too many levels:can't expand\n");
#endif
	  return(TRUE);
	}
	else
	  {
#ifdef DEBUG	      
	    eputs("add_tri_expand():too many tris inset:can't add\n");
#endif
	    return(FALSE);
	  }
}


int
add_tri(qtptr,fptr,t_id,del_set)
   QUADTREE *qtptr;
   int *fptr;
   int t_id;
   OBJECT *del_set;
{

  OBJECT *optr,*tptr;
  OBJECT t_set[QT_MAXSET +1],r_set[QT_MAXSET +1];
  int i,id,found;
#ifdef DEBUG_TEST_DRIVER
    Pick_tri = t_id;
    Picking = TRUE;
#endif    
    if(QT_IS_EMPTY(*qtptr))
    {
	*qtptr = qtaddelem(*qtptr,t_id);
	if(!QT_FLAG_FILL_TRI(*fptr))
	  (*fptr)++;
    }
    else
     {
	 optr = qtqueryset(*qtptr);
	 if(del_set)
	  {
	      setintersect(r_set,del_set,optr);
	      if(QT_SET_CNT(r_set) > 0)
	      {
		  qtgetset(t_set,*qtptr);
		  optr = QT_SET_PTR(r_set);
		  for(i = QT_SET_CNT(r_set); i > 0; i--)
		     {
			 id = QT_SET_NEXT_ELEM(optr);
			 deletelem(t_set,id);
		     }
		  qtfreeleaf(*qtptr);
		  *qtptr = qtnewleaf(t_set);
		  optr = t_set;
	      }
	  }
	 if(!inset(optr,t_id))
	 {
	     if(QT_SET_CNT(optr) < QT_MAXSET)
	     {
		 if(QT_SET_CNT(optr) >= QT_SET_THRESHOLD)
		    (*fptr) |= QT_EXPAND;
		 if(!QT_FLAG_FILL_TRI(*fptr))
		    (*fptr)++;
		 *qtptr = qtaddelem(*qtptr,t_id);
	     }
	     else
		{
#ifdef DEBUG_TESTDRIVER	      
		    eputs("add_tri():exceeded set size\n");
#endif
		    return(FALSE);
		}
	 }
     }
    return(TRUE);
}


smLocator_add_tri(sm,t_id,v0_id,v1_id,v2_id,del_set)
SM *sm;
int t_id;
int v0_id,v1_id,v2_id;
OBJECT *del_set;
{
  STREE *st;
  FVECT v0,v1,v2;
  
  st = SM_LOCATOR(sm);

  VSUB(v0,SM_NTH_WV(sm,v0_id),SM_VIEW_CENTER(sm));
  VSUB(v1,SM_NTH_WV(sm,v1_id),SM_VIEW_CENTER(sm));
  VSUB(v2,SM_NTH_WV(sm,v2_id),SM_VIEW_CENTER(sm));

  qtClearAllFlags();
  
  stApply_to_tri(st,v0,v1,v2,add_tri,add_tri_expand,t_id,del_set);

}

/* Add a triangle to the base array with vertices v1-v2-v3 */
int
smAdd_tri(sm, v0_id,v1_id,v2_id,tptr)
SM *sm;
int v0_id,v1_id,v2_id;
TRI **tptr;
{
    int t_id;
    TRI *t;


    if(SM_TRI_CNT(sm)+1 > SM_MAX_TRIS(sm))
      error(SYSTEM,"smAdd_tri():Too many triangles");

    /* Get the id for the next available triangle */
    SM_FREE_TRI_ID(sm,t_id);
    if(t_id == -1)
      return(t_id);

    t = SM_NTH_TRI(sm,t_id);
    T_CLEAR_NBRS(t);

    if(SM_BASE_ID(sm,v0_id) || SM_BASE_ID(sm,v1_id) || SM_BASE_ID(sm,v2_id))
    {
      smClear_tri_flags(sm,t_id);
      SM_SET_NTH_T_BASE(sm,t_id);
    }
    else
    {
      SM_CLEAR_NTH_T_BASE(sm,t_id);
      SM_SET_NTH_T_ACTIVE(sm,t_id);
      SM_SET_NTH_T_LRU(sm,t_id);
      SM_SET_NTH_T_NEW(sm,t_id);
      SM_NUM_TRIS(sm)++;
      smNew_tri_cnt++;
    }
    /* set the triangle vertex ids */
    T_NTH_V(t,0) = v0_id;
    T_NTH_V(t,1) = v1_id;
    T_NTH_V(t,2) = v2_id;

    SM_NTH_VERT(sm,v0_id) = t_id;
    SM_NTH_VERT(sm,v1_id) = t_id;
    SM_NTH_VERT(sm,v2_id) = t_id;

    if(t)
       *tptr = t;
    /* return initialized triangle */
    return(t_id);
}

int
smClosest_vertex_in_tri(sm,v0_id,v1_id,v2_id,p,eps)
SM *sm;
int v0_id,v1_id,v2_id;
FVECT p;
double eps;
{
    FVECT v;
    double d,d0,d1,d2;
    int closest = -1;

    if(v0_id != -1)
    {
      smDir(sm,v,v0_id);
      d0 = DIST(p,v);
      if(eps < 0 || d0 < eps)
      {
	closest = v0_id;
	d = d0;
      }
    }
    if(v1_id != -1)
    {
      smDir(sm,v,v1_id);
      d1 = DIST(p,v);
      if(closest== -1)
      {
	  if(eps < 0 || d1 < eps)
	   {
	       closest = v1_id;
	       d = d1;
	   }
      }
      else
	if(d1 < d0)
        {
	  if((eps < 0) ||  d1 < eps)
	  {
	    closest = v1_id;
	    d = d1;
	  }
	}
    }
    if(v2_id != -1)
    {
      smDir(sm,v,v2_id);
      d2 = DIST(p,v);
      if((eps < 0) ||  d2 < eps)
	 if(closest == -1 ||(d2 < d))
	     return(v2_id);
    }
    return(closest);
}


int
smClosest_vertex_in_w_tri(sm,v0_id,v1_id,v2_id,p)
SM *sm;
int v0_id,v1_id,v2_id;
FVECT p;
{
    FVECT v;
    double d,d0,d1,d2;
    int closest;

    VCOPY(v,SM_NTH_WV(sm,v0_id));
    d = d0 = DIST(p,v);
    closest = v0_id;
    
    VCOPY(v,SM_NTH_WV(sm,v1_id));
    d1 = DIST(p,v);
    if(d1 < d0)
    {
      closest = v1_id;
      d = d1;
    }
    VCOPY(v,SM_NTH_WV(sm,v2_id));
    d2 = DIST(p,v);
    if(d2 < d)
      return(v2_id);
    else
      return(closest);
}

void
smTris_swap_edge(sm,t_id,t1_id,e,e1,tn_id,tn1_id,add_ptr,del_set)
   SM *sm;
   int t_id,t1_id;
   int e,e1;
   int *tn_id,*tn1_id;
   LIST **add_ptr;
   OBJECT *del_set;

{
    TRI *t,*t1;
    TRI *ta,*tb;
    int verts[3],enext,eprev,e1next,e1prev;
    TRI *n;
    FVECT p1,p2,p3;
    int ta_id,tb_id;
    /* swap diagonal (e relative to t, and e1 relative to t1)
      defined by quadrilateral
      formed by t,t1- swap for the opposite diagonal
   */
    t = SM_NTH_TRI(sm,t_id);
    t1 = SM_NTH_TRI(sm,t1_id);
    enext = (e+1)%3;
    eprev = (e+2)%3;
    e1next = (e1+1)%3;
    e1prev = (e1+2)%3;
    verts[e] = T_NTH_V(t,e);
    verts[enext] = T_NTH_V(t1,e1prev);
    verts[eprev] = T_NTH_V(t,eprev);
    ta_id = smAdd_tri(sm,verts[0],verts[1],verts[2],&ta);
    *add_ptr = push_data(*add_ptr,ta_id);
    verts[e1] = T_NTH_V(t1,e1);
    verts[e1next] = T_NTH_V(t,eprev);
    verts[e1prev] = T_NTH_V(t1,e1prev);
    tb_id = smAdd_tri(sm,verts[0],verts[1],verts[2],&tb);
    *add_ptr = push_data(*add_ptr,tb_id);

    /* set the neighbors */
    T_NTH_NBR(ta,e) = T_NTH_NBR(t1,e1next);
    T_NTH_NBR(tb,e1) = T_NTH_NBR(t,enext);
    T_NTH_NBR(ta,enext) = tb_id;
    T_NTH_NBR(tb,e1next) = ta_id;
    T_NTH_NBR(ta,eprev) = T_NTH_NBR(t,eprev);
    T_NTH_NBR(tb,e1prev) = T_NTH_NBR(t1,e1prev);    

    /* Reset neighbor pointers of original neighbors */
    n = SM_NTH_TRI(sm,T_NTH_NBR(t,enext));
    T_NTH_NBR(n,T_NTH_NBR_PTR(t_id,n)) = tb_id;
    n = SM_NTH_TRI(sm,T_NTH_NBR(t,eprev));
    T_NTH_NBR(n,T_NTH_NBR_PTR(t_id,n)) = ta_id;

    n = SM_NTH_TRI(sm,T_NTH_NBR(t1,e1next));
    T_NTH_NBR(n,T_NTH_NBR_PTR(t1_id,n)) = ta_id;
    n = SM_NTH_TRI(sm,T_NTH_NBR(t1,e1prev));
    T_NTH_NBR(n,T_NTH_NBR_PTR(t1_id,n)) = tb_id;

    /* Delete two parent triangles */
    if(remove_from_list(t_id,add_ptr))
       smDelete_tri(sm,t_id); 
    else
       insertelem(del_set,t_id);

    if(remove_from_list(t1_id,add_ptr))
       smDelete_tri(sm,t1_id); 
    else
       insertelem(del_set,t1_id);

    *tn_id = ta_id;
    *tn1_id = tb_id;
}

smUpdate_locator(sm,add_list,del_set)
SM *sm;
LIST *add_list;
OBJECT *del_set;
{
  int t_id,i;
  TRI *t;
  OBJECT *optr;
  
  while(add_list)
  {
    t_id = pop_list(&add_list);
    t = SM_NTH_TRI(sm,t_id);
    smLocator_add_tri(sm,t_id,T_NTH_V(t,0),T_NTH_V(t,1),T_NTH_V(t,2),del_set);
  }

  optr = QT_SET_PTR(del_set);
  for(i = QT_SET_CNT(del_set); i > 0; i--)
  {
      t_id = QT_SET_NEXT_ELEM(optr);
      smDelete_tri(sm,t_id); 
  }
}
/* MUST add check for constrained edges */
int
smFix_tris(sm,id,tlist,add_list,del_set)
SM *sm;
int id;
LIST *tlist;
LIST *add_list;
OBJECT *del_set;
{
    TRI *t,*t_opp;
    FVECT p,p1,p2,p3;
    int e,e1,swapped = 0;
    int t_id,t_opp_id;


    VSUB(p,SM_NTH_WV(sm,id),SM_VIEW_CENTER(sm));
    while(tlist)
    {
	t_id = pop_list(&tlist);
        t = SM_NTH_TRI(sm,t_id);
        e = (T_WHICH_V(t,id)+1)%3;
        t_opp_id = T_NTH_NBR(t,e);
        t_opp = SM_NTH_TRI(sm,t_opp_id);
	/*
	VSUB(p1,SM_T_NTH_WV(sm,t_opp,0),SM_VIEW_CENTER(sm));
	VSUB(p2,SM_T_NTH_WV(sm,t_opp,1),SM_VIEW_CENTER(sm));
	VSUB(p3,SM_T_NTH_WV(sm,t_opp,2),SM_VIEW_CENTER(sm));
	*/
	smDir_in_cone(sm,p1,T_NTH_V(t_opp,0));
	smDir_in_cone(sm,p2,T_NTH_V(t_opp,1));
	smDir_in_cone(sm,p3,T_NTH_V(t_opp,2));
	if(point_in_cone(p,p1,p2,p3))
	{
	    swapped = 1;
	    e1 = T_NTH_NBR_PTR(t_id,t_opp);
	    /* check list for t_opp and Remove if there */
	    remove_from_list(t_opp_id,&tlist);
	    smTris_swap_edge(sm,t_id,t_opp_id,e,e1,&t_id,&t_opp_id,
			     &add_list,del_set);
	    tlist = push_data(tlist,t_id);
	    tlist = push_data(tlist,t_opp_id);
	}
    }
    smUpdate_locator(sm,add_list,del_set);
    return(swapped);
}

/* Give the vertex "id" and a triangle "t" that it belongs to- return the
   next nbr in a counter clockwise order about vertex "id"
*/
int
smTri_next_ccw_nbr(sm,t,id)
SM *sm;
TRI *t;
int id;
{
  int t_id;
  int tri;

  /* Want the edge for which "id" is the destination */
  t_id = (T_WHICH_V(t,id)+ 2)% 3;
  tri = T_NTH_NBR(t,t_id);
  return(tri);
}

void
smReplace_point(sm,tri,id,nid)
SM *sm;
TRI *tri;
int id,nid;
{
  TRI *t;
  int t_id;
  
  T_NTH_V(tri,T_WHICH_V(tri,id)) = nid;

  t_id = smTri_next_ccw_nbr(sm,tri,nid);
  while((t = SM_NTH_TRI(sm,t_id)) != tri)
  {
      T_NTH_V(t,T_WHICH_V(t,id)) = nid;
      t_id = smTri_next_ccw_nbr(sm,t,nid);
  }
} 


smClear_tri_flags(sm,id)
SM *sm;
int id;
{
  int i;

  for(i=0; i < T_FLAGS; i++)
    SM_CLEAR_NTH_T_FLAG(sm,id,i);

}

/* Locate the point-id in the point location structure: */
int
smReplace_vertex(sm,c,dir,p,tri_id,snew_id,type,which)
   SM *sm;
   COLR c;
   FVECT dir,p;
   int tri_id,snew_id;
   int type,which;
{
    int n_id,s_id;
    TRI *tri;

    tri = SM_NTH_TRI(sm,tri_id);
    /* Get the sample that corresponds to the "which" vertex of "tri" */
    /* NEED: have re-init that sets clock flag */
    /* If this is a base-sample: create a new sample and replace
       all references to the base sample with references to the
       new sample
       */
    s_id = T_NTH_V(tri,which);
    if(SM_BASE_ID(sm,s_id))
     {
	 if(snew_id != -1)
	   n_id = smAdd_sample_point(sm,c,dir,p);
	 else
	   n_id = snew_id;
	 smReplace_point(sm,tri,s_id,n_id); 
	 s_id = n_id;
     }
    else /* If the sample exists, reset the values */
       /* NOTE: This test was based on the SPHERICAL coordinates
	       of the point: If we are doing a multiple-sample-per
	       SPHERICAL pixel: we will want to test for equality-
	       and do other processing: for now: SINGLE SAMPLE PER
	       PIXEL
	       */
      /* NOTE: snew_id needs to be marked as invalid?*/
      if(snew_id == -1)
	smInit_sample(sm,s_id,c,dir,p);
    else
      smReset_sample(sm,s_id,snew_id);
    return(s_id);
}


/* Locate the point-id in the point location structure: */
int
smInsert_point_in_tri(sm,c,dir,p,s_id,tri_id)
   SM *sm;
   COLR c;
   FVECT dir,p;
   int s_id,tri_id;
{
    TRI *tri,*t0,*t1,*t2,*nbr;
    int v0_id,v1_id,v2_id,n_id;
    int t0_id,t1_id,t2_id;
    LIST *tlist,*add_list;
    OBJECT del_set[QT_MAXSET+1]; 
    FVECT npt;

    add_list = NULL;
    QT_CLEAR_SET(del_set);
    if(s_id == SM_INVALID)
       s_id = smAdd_sample_point(sm,c,dir,p);
    
    tri = SM_NTH_TRI(sm,tri_id);
    v0_id = T_NTH_V(tri,0);
    v1_id = T_NTH_V(tri,1);
    v2_id = T_NTH_V(tri,2);

    n_id = -1;
    if(SM_BASE_ID(sm,v0_id)||SM_BASE_ID(sm,v1_id)||SM_BASE_ID(sm,v2_id))
    {
      smDir(sm,npt,s_id);
	    /* Change to an add and delete */
      t0_id = (SM_BASE_ID(sm,v0_id))?v0_id:-1;
      t1_id = (SM_BASE_ID(sm,v1_id))?v1_id:-1;
      t2_id = (SM_BASE_ID(sm,v2_id))?v2_id:-1;	
      n_id = smClosest_vertex_in_tri(sm,t0_id,t1_id,t2_id,npt,P_REPLACE_EPS);
    }
    t0_id = smAdd_tri(sm,s_id,v0_id,v1_id,&t0);
    /* Add triangle to the locator */
    
    add_list = push_data(add_list,t0_id);

    t1_id = smAdd_tri(sm,s_id,v1_id,v2_id,&t1);	
    add_list = push_data(add_list,t1_id);

    t2_id = smAdd_tri(sm,s_id,v2_id,v0_id,&t2);
    add_list = push_data(add_list,t2_id);

    /* Set the neighbor pointers for the new tris */
    T_NTH_NBR(t0,0) = t2_id;
    T_NTH_NBR(t0,1) = T_NTH_NBR(tri,0);
    T_NTH_NBR(t0,2) = t1_id;
    T_NTH_NBR(t1,0) = t0_id;
    T_NTH_NBR(t1,1) = T_NTH_NBR(tri,1);
    T_NTH_NBR(t1,2) = t2_id;
    T_NTH_NBR(t2,0) = t1_id;
    T_NTH_NBR(t2,1) = T_NTH_NBR(tri,2);
    T_NTH_NBR(t2,2) = t0_id;

    /* Reset the neigbor pointers for the neighbors of the original */
    nbr = SM_NTH_TRI(sm,T_NTH_NBR(tri,0));
    T_NTH_NBR(nbr,T_NTH_NBR_PTR(tri_id,nbr)) = t0_id;
    nbr = SM_NTH_TRI(sm,T_NTH_NBR(tri,1));
    T_NTH_NBR(nbr,T_NTH_NBR_PTR(tri_id,nbr)) = t1_id;
    nbr = SM_NTH_TRI(sm,T_NTH_NBR(tri,2));
    T_NTH_NBR(nbr,T_NTH_NBR_PTR(tri_id,nbr)) = t2_id;
	
    insertelem(del_set,tri_id);

    /* Fix up the new triangles*/
    tlist = push_data(NULL,t0_id);
    tlist = push_data(tlist,t1_id);
    tlist = push_data(tlist,t2_id);

    smFix_tris(sm,s_id,tlist,add_list,del_set);

    if(n_id != -1)
       smDelete_point(sm,n_id);

    return(s_id);
}
    

int
smPointLocate(sm,pt,norm)
SM *sm;
FVECT pt;
int norm;
{
  STREE *st;
  int tri;
  FVECT npt;
  
  st = SM_LOCATOR(sm);
  if(norm)
  {
      VSUB(npt,pt,SM_VIEW_CENTER(sm));
      tri = stPoint_locate(st,npt);
  }
  else
     tri = stPoint_locate(st,pt);
  return(tri);
}

QUADTREE
smPointLocateCell(sm,pt,norm,v0,v1,v2)
SM *sm;
FVECT pt;
int norm;
FVECT v0,v1,v2;
{
  STREE *st;
  QUADTREE *qtptr;
  FVECT npt;

  st = SM_LOCATOR(sm);
  if(norm)
  {
      VSUB(npt,pt,SM_VIEW_CENTER(sm));
  
      qtptr = stPoint_locate_cell(st,npt,v0,v1,v2);
  }
  else
     qtptr = stPoint_locate_cell(st,pt,v0,v1,v2);

  if(qtptr)
    return(*qtptr);
  else
    return(EMPTY);
}

int
smAdd_sample_to_mesh(sm,c,dir,pt,s_id)
   SM *sm;
   COLR c;
   FVECT dir,pt;
   int s_id;
{
    int t_id;
    double d;
    FVECT p;
    
    /* If new, foreground pt */
    if(pt)
    {
	/* NOTE: This should be elsewhere! */
	d = DIST(pt,SM_VIEW_CENTER(smMesh));
	smDist_sum += 1.0/d;
	/************************************/
	t_id = smPointLocate(smMesh,pt,TRUE); 
	if(t_id >= 0)
	   s_id = smInsert_point_in_tri(smMesh,c,dir,pt,s_id,t_id);
#ifdef DEBUG
	   else
	     {
	       c[0] = 255;c[1]=0;c[2]=0;
	       s_id = smAdd_sample_point(sm,c,dir,pt);	       
	       eputs("smAdd_sample_to_mesh(): not found fg\n");
	     }
#endif
    }
    else if(s_id != -1)
    {
	VCOPY(p,SM_NTH_WV(sm,s_id));
	if(SM_NTH_W_DIR(sm,s_id) != -1)
	{
	    /* NOTE: This should be elsewhere! */
	    d = DIST(p,SM_VIEW_CENTER(smMesh));
	    smDist_sum += 1.0/d;
	    /************************************/
	}
	t_id = smPointLocate(smMesh,p,TRUE); 
	if(t_id != -1)
	   s_id = smInsert_point_in_tri(smMesh,c,dir,p,s_id,t_id);
#ifdef DEBUG
	   else
	      eputs("smAdd_sample_to_mesh():not found reinsert\n");
#endif
    }
    /* Is a BG(sky point) */
    else
       {
	   t_id = smPointLocate(smMesh,dir,FALSE); 
	   if(t_id != -1)
	      s_id = smInsert_point_in_tri(smMesh,c,dir,NULL,s_id,t_id);

#ifdef DEBUG
	      else
		 eputs("smAdd_sample_to_mesh(): not found bg\n");
#endif
       }
    return(s_id);
}

/*
 * int
 * smNewSamp(c, dir, p)	: register new sample point and return index
 * COLR	c;		: pixel color (RGBE)
 * FVECT	dir;	: ray direction vector
 * FVECT	p;	: world intersection point
 *
 * Add new sample point to data structures, removing old values as necessary.
 * New sample representation will be output in next call to smUpdate().
 * If the point is a sky point: then v=NULL
 */
int
smNewSamp(c,dir,p)
COLR c;
FVECT dir;
FVECT p;

{
    int s_id;
    int debug=0;
    
    /* First check if this the first sample: if so initialize mesh */
    if(SM_NUM_SAMP(smMesh) == 0)
      smInit_mesh(smMesh,odev.v.vp);
    if(!debug)
       s_id = smAdd_sample_to_mesh(smMesh,c,dir,p,-1);
    return(s_id);
    
 }    
/*
 * int
 * smFindsamp(orig, dir): intersect ray with 3D rep. and find closest sample
 * FVECT	orig, dir;
 *
 * Find the closest sample to the given ray.  Return -1 on failure.
 */

/*
 * smClean()		: display has been wiped clean
 *
 * Called after display has been effectively cleared, meaning that all
 * geometry must be resent down the pipeline in the next call to smUpdate().
 */


/*
 * smUpdate(vp, qua)	: update OpenGL output geometry for view vp
 * VIEW	*vp;		: desired view
 * int	qua;		: quality level (percentage on linear time scale)
 *
 * Draw new geometric representation using OpenGL calls.  Assume that the
 * view has already been set up and the correct frame buffer has been
 * selected for drawing.  The quality level is on a linear scale, where 100%
 * is full (final) quality.  It is not necessary to redraw geometry that has
 * been output since the last call to smClean().
 */


int
smClear_vert(sm,id)
SM *sm;
int id;
{
    if(SM_INVALID_POINT_ID(sm,id))
       return(FALSE);
    
    SM_NTH_VERT(sm,id) = SM_INVALID;

    return(TRUE);
}

int
smAdd_base_vertex(sm,v,d)
   SM *sm;
   FVECT v,d;
{
  int id;

  /* First add coordinate to the sample array */
  id = smAdd_aux_point(sm,v,d);
  if(id == -1)
    return(SM_INVALID);
  /* Initialize triangle pointer to -1 */
  smClear_vert(sm,id);
  return(id);
}



/* Initialize a the point location DAG based on a 6 triangle tesselation
   of the unit sphere centered on the view center. The DAG structure
   contains 6 roots: one for each initial base triangle
*/

int
smCreate_base_mesh(sm,type)
SM *sm;
int type;
{
  int i,id,tri_id,nbr_id;
  int p[4],ids[4];
  int v0_id,v1_id,v2_id;
  TRI *tris[4];
  FVECT d,pt,cntr;
  
  /* First insert the base vertices into the sample point array */

  for(i=0; i < 4; i++)
  {
    VCOPY(cntr,stDefault_base[i]);
    cntr[0] += .01;
    cntr[1] += .02;
    cntr[2] += .03;
    VADD(cntr,cntr,SM_VIEW_CENTER(sm));
    d[0] = -1;
    id = smAdd_base_vertex(sm,cntr,d);
    /* test to make sure vertex allocated */
    if(id != -1)
      p[i] = id;
    else
      return(0);
  }
  /* Create the base triangles */
  for(i=0;i < 4; i++)
  {
    v0_id = p[stTri_verts[i][0]];
    v1_id = p[stTri_verts[i][1]];
    v2_id = p[stTri_verts[i][2]];
    if((ids[i] = smAdd_tri(sm, v0_id,v1_id,v2_id,&(tris[i])))== -1)
     return(0);
    smLocator_add_tri(sm,ids[i],v0_id,v1_id,v2_id,NULL);
  }
  /* Set neighbors */

  for(tri_id=0;tri_id < 4; tri_id++)
     for(nbr_id=0; nbr_id < 3; nbr_id++)
	T_NTH_NBR(tris[tri_id],nbr_id) = smBase_nbrs[tri_id][nbr_id];

  return(1);

}


int
smNext_tri_flag_set(sm,i,which,b)
     SM *sm;
     int i,which;
     int b;
{

  for(; i < SM_TRI_CNT(sm);i++)
  {

      if(!SM_IS_NTH_T_FLAG(sm,i,which))
	 continue;
    if(!b)
      break;
    if((b==1) && !SM_BG_TRI(sm,i))
      break;
    if((b==2) && SM_BG_TRI(sm,i))
      break;
  }
     
  return(i);
}


int
smNext_valid_tri(sm,i)
     SM *sm;
     int i;
{

  while( i < SM_TRI_CNT(sm) && !T_IS_VALID(SM_NTH_TRI(sm,i)))
     i++;
     
  return(i);
}



qtTri_from_id(t_id,v0,v1,v2,n0,n1,n2,v0_idp,v1_idp,v2_idp) 
int t_id;
FVECT v0,v1,v2;
FVECT n0,n1,n2;
int *v0_idp,*v1_idp,*v2_idp;
{
  TRI *t;
  int v0_id,v1_id,v2_id;
  
  t = SM_NTH_TRI(smMesh,t_id);
  v0_id = T_NTH_V(t,0);
  v1_id = T_NTH_V(t,1);
  v2_id = T_NTH_V(t,2);

  if(v0)
  {
      VSUB(v0,SM_NTH_WV(smMesh,v0_id),SM_VIEW_CENTER(smMesh));
      VSUB(v1,SM_NTH_WV(smMesh,v1_id),SM_VIEW_CENTER(smMesh));
      VSUB(v2,SM_NTH_WV(smMesh,v2_id),SM_VIEW_CENTER(smMesh));
  }
  if(n0)
  {
      smDir(smMesh,n0,v0_id);
      smDir(smMesh,n1,v1_id);
      smDir(smMesh,n2,v2_id);

  }
  if(v0_idp)
     {
	 *v0_idp = v0_id;
	 *v1_idp = v1_id;
	 *v2_idp = v2_id;
     }
}


/*
 * int
 * smFindSamp(FVECT orig, FVECT dir) 
 *
 * Find the closest sample to the given ray.  Returns sample id, -1 on failure.
 * "dir" is assumed to be normalized
 */

  

smRebuild_mesh(sm,vp)
   SM *sm;
   FVECT vp;
{
    int i;
    FVECT dir;
    COLR c;
    FVECT p,ov;

    /* Clear the mesh- and rebuild using the current sample array */

    VSUB(ov,vp,SM_VIEW_CENTER(sm));
    smInit_mesh(sm,vp);
    
    SM_FOR_ALL_SAMPLES(sm,i)
    {
	if(SM_NTH_W_DIR(sm,i)==-1)
	   VADD(SM_NTH_WV(sm,i),SM_NTH_WV(sm,i),ov);	    
	smAdd_sample_to_mesh(sm,NULL,NULL,NULL,i);	
    }
}

int
intersect_tri_set(t_set,orig,dir,pt)
   OBJECT *t_set;
   FVECT orig,dir,pt;
{
    OBJECT *optr;
    int i,t_id,id;
    int pid0,pid1,pid2;
    FVECT p0,p1,p2,p;
    TRI *t;
    
    optr = QT_SET_PTR(t_set);
    for(i = QT_SET_CNT(t_set); i > 0; i--)
    {
	t_id = QT_SET_NEXT_ELEM(optr);

	t = SM_NTH_TRI(smMesh,t_id);
	pid0 = T_NTH_V(t,0);
	pid1 = T_NTH_V(t,1);
	pid2 = T_NTH_V(t,2);
	VCOPY(p0,SM_NTH_WV(smMesh,pid0));
	VCOPY(p1,SM_NTH_WV(smMesh,pid1));
	VCOPY(p2,SM_NTH_WV(smMesh,pid2));
	if(ray_intersect_tri(orig,dir,p0,p1,p2,p))
        {
	  id = closest_point_in_tri(p0,p1,p2,p,pid0,pid1,pid2);

	  if(pt)
	     VCOPY(pt,p);
#ifdef DEBUG_TEST_DRIVER
	  Pick_tri = t_id;
	  Pick_samp = id;
	  VCOPY(Pick_point[0],p);
#endif
	  return(id);
	}
    }
    return(-1);
}

int
ray_trace_check_set(qtptr,orig,dir,tptr,os)
   QUADTREE *qtptr;
   FVECT orig,dir;
   int *tptr;
   OBJECT *os;
{
    OBJECT tset[QT_MAXSET+1];	
    double dt,t;
    int found;
    FVECT o;
    
 
    if(!QT_IS_EMPTY(*qtptr))
     {
	 VADD(o,orig,SM_VIEW_CENTER(smMesh));
	 qtgetset(tset,*qtptr);
	 /* Check triangles in set against those seen so far(os):only
	    check new triangles for intersection (t_set') 
	    */
	 check_set(tset,os);
	 if((found = intersect_tri_set(tset,o,dir,NULL))!= -1)
         {
	     *tptr = found;
	     return(QT_DONE);
	 }
       }
    return(FALSE);
}

int
smFindSamp(orig,dir)
FVECT orig,dir;
{
  FVECT b,p,o;
  OBJECT *ts;
  QUADTREE qt;
  int s_id;
  double d;

 /*  r is the normalized vector from the view center to the current
  *  ray point ( starting with "orig"). Find the cell that r falls in,
  *  and test the ray against all triangles stored in the cell. If
  *  the test fails, trace the projection of the ray across to the
  *  next cell it intersects: iterate until either an intersection
  *  is found, or the projection ray is // to the direction. The sample
  *  corresponding to the triangle vertex closest to the intersection
  *  point is returned.
  */
  
  /* First test if "orig" coincides with the View_center or if "dir" is
     parallel to r formed by projecting "orig" on the sphere. In
     either case, do a single test against the cell containing the
     intersection of "dir" and the sphere
   */
  /* orig will be updated-so preserve original value */
  if(!smMesh)
     return;
  point_on_sphere(b,orig,SM_VIEW_CENTER(smMesh));
  d = -DOT(b,dir);
  if(EQUAL_VEC3(orig,SM_VIEW_CENTER(smMesh)) || EQUAL(fabs(d),1.0))
  {
      qt = smPointLocateCell(smMesh,dir,FALSE,NULL,NULL,NULL);
      /* Test triangles in the set for intersection with Ray:returns
	 first found
      */
      ts = qtqueryset(qt);
      s_id = intersect_tri_set(ts,orig,dir,p);
#ifdef DEBUG_TEST_DRIVER
      VCOPY(Pick_point[0],p);
#endif
  }
  else
  {
    OBJECT t_set[QT_MAXCSET+1];
    /* Test each of the root triangles against point id */
    QT_CLEAR_SET(t_set);
    VSUB(o,orig,SM_VIEW_CENTER(smMesh));
    ST_CLEAR_FLAGS(SM_LOCATOR(smMesh));
    s_id=stTrace_ray(SM_LOCATOR(smMesh),o,dir,ray_trace_check_set,&s_id,t_set);
 }    
  return(s_id);
}











