#include "assemble.h"

// C++ include files that we need
#include <iostream>
#include <algorithm>
#include <math.h>
#include <time.h>
// Basic include file needed for the mesh functionality.
#include "libmesh.h"
#include "mesh.h"
#include "mesh_generation.h"
#include "exodusII_io.h"
#include "equation_systems.h"
#include "fe.h"
#include "quadrature_gauss.h"
#include "dof_map.h"
#include "sparse_matrix.h"
#include "numeric_vector.h"
#include "dense_matrix.h"
#include "dense_vector.h"
#include "linear_implicit_system.h"
#include "transient_system.h"
// For systems of equations the \p DenseSubMatrix
// and \p DenseSubVector provide convenient ways for
// assembling the element matrix and vector on a
// component-by-component basis.
#include "dense_submatrix.h"
#include "dense_subvector.h"
// The definition of a geometric element
#include "elem.h"
#include "assemble.h"

#define E 1
#define NU 0.15
#define KPERM 0.1

void assemble_stokes (EquationSystems& es,
                      const std::string& system_name)
{

Real mu = E/(2*(1+NU));
Real lambda = (E*NU)/((1+NU)*(1-2*NU));

//std::cout<< mu <<std::endl;
//std::cout<< lambda <<std::endl;

  PerfLog perf_log("Assemble");

  // It is a good idea to make sure we are assembling
  // the proper system.
  libmesh_assert (system_name == "Last_non_linear_soln");
  
  const Real dt    = es.parameters.get<Real>("dt");
  const Real progress    = es.parameters.get<Real>("progress");
  const Real time    = es.parameters.get<Real>("time");

  // Get a constant reference to the mesh object.
  const MeshBase& mesh = es.get_mesh();
  
  // The dimension that we are running
  const unsigned int dim = mesh.mesh_dimension();
  
  // Get a reference to the Convection-Diffusion system object.
  TransientLinearImplicitSystem & system =
    es.get_system<TransientLinearImplicitSystem> ("Last_non_linear_soln");

 //TransientLinearImplicitSystem & system =    es.get_system<TransientLinearImplicitSystem> ("Stokes");
  // Numeric ids corresponding to each variable in the system
  const unsigned int u_var = system.variable_number ("s_u");
  const unsigned int v_var = system.variable_number ("s_v");
  #if THREED
  const unsigned int w_var = system.variable_number ("s_w");
  #endif
  const unsigned int p_var = system.variable_number ("s_p");
  const unsigned int x_var = system.variable_number ("x");
  const unsigned int y_var = system.variable_number ("y");
   #if THREED
  const unsigned int z_var = system.variable_number ("z");
  #endif
  // Get the Finite Element type for "u".  Note this will be
  // the same as the type for "v".
  FEType fe_disp_type = system.variable_type(u_var);
  FEType fe_vel_type = system.variable_type(x_var);

  // Get the Finite Element type for "p".
  FEType fe_pres_type = system.variable_type(p_var);

  // Build a Finite Element object of the specified type for
  // the velocity variables.
  AutoPtr<FEBase> fe_disp  (FEBase::build(dim, fe_disp_type));
  AutoPtr<FEBase> fe_vel  (FEBase::build(dim, fe_vel_type));
    
  // Build a Finite Element object of the specified type for
  // the pressure variables.
  AutoPtr<FEBase> fe_pres (FEBase::build(dim, fe_pres_type));
  
  // A Gauss quadrature rule for numerical integration.
  // Let the \p FEType object decide what order rule is appropriate.
  QGauss qrule (dim, fe_vel_type.default_quadrature_order());

  // Tell the finite element objects to use our quadrature rule.
  fe_disp->attach_quadrature_rule (&qrule);
  fe_vel->attach_quadrature_rule (&qrule);
  fe_pres->attach_quadrature_rule (&qrule);
  
  // Here we define some references to cell-specific data that
  // will be used to assemble the linear system.
  //
  // The element Jacobian * quadrature weight at each integration point.   
  const std::vector<Real>& JxW = fe_vel->get_JxW();
  
  const std::vector<Point>& q_point = fe_vel->get_xyz();

  // The element shape function gradients for the velocity
  // variables evaluated at the quadrature points.
  const std::vector<std::vector<RealGradient> >& dphi = fe_disp->get_dphi();
  const std::vector<std::vector<Real> >& phi = fe_disp->get_phi();
  const std::vector<std::vector<RealGradient> >& f_dphi = fe_vel->get_dphi();
  const std::vector<std::vector<Real> >& f_phi = fe_vel->get_phi();

  // The element shape functions for the pressure variable
  // evaluated at the quadrature points.
  const std::vector<std::vector<Real> >& psi = fe_pres->get_phi();
  const std::vector<std::vector<RealGradient> >& dpsi = fe_pres->get_dphi();


#if NITSCHE
	AutoPtr<FEBase> fe_face_d (FEBase::build(dim, fe_disp_type));          
	AutoPtr<QBase> qface_d(fe_disp_type.default_quadrature_rule(dim-1));
	fe_face_d->attach_quadrature_rule (qface_d.get());

	AutoPtr<FEBase> fe_face_f (FEBase::build(dim, fe_vel_type));          
	AutoPtr<QBase> qface_f(fe_vel_type.default_quadrature_rule(dim-1));
	fe_face_f->attach_quadrature_rule (qface_f.get());

	AutoPtr<FEBase> fe_face_p (FEBase::build(dim, fe_pres_type));          
	AutoPtr<QBase> qface_p(fe_pres_type.default_quadrature_rule(dim-1));
	fe_face_p->attach_quadrature_rule (qface_p.get());

	//AutoPtr<FEBase> fe_face_ref (FEBase::build(dim, fe_vel_type_ref));          
	//AutoPtr<QBase> qface_ref(fe_vel_type_ref.default_quadrature_rule(dim-1));
	//fe_face_ref->attach_quadrature_rule (qface_ref.get());
#endif

  const DofMap & dof_map = system.get_dof_map();

  // Define data structures to contain the element matrix
  // and right-hand-side vector contribution.  Following
  // basic finite element terminology we will denote these
  // "Ke" and "Fe".
  DenseMatrix<Number> Ke;
  DenseVector<Number> Fe;

  DenseMatrix<Number> Kstab;

#if !THREED
  DenseSubMatrix<Number>
    Kuu(Ke), Kuv(Ke), Kup(Ke), Kux(Ke), Kuy(Ke),
    Kvu(Ke), Kvv(Ke), Kvp(Ke), Kvx(Ke), Kvy(Ke),
    Kpu(Ke), Kpv(Ke), Kpp(Ke), Kpx(Ke), Kpy(Ke),
    Kxu(Ke), Kxv(Ke), Kxp(Ke), Kxx(Ke), Kxy(Ke),
    Kyu(Ke), Kyv(Ke), Kyp(Ke), Kyx(Ke), Kyy(Ke);
#endif

#if THREED
  DenseSubMatrix<Number>
    Kuu(Ke), Kuv(Ke), Kuw(Ke), Kup(Ke), Kux(Ke), Kuy(Ke), Kuz(Ke),
    Kvu(Ke), Kvv(Ke), Kvw(Ke), Kvp(Ke), Kvx(Ke), Kvy(Ke), Kvz(Ke),
    Kwu(Ke), Kwv(Ke), Kww(Ke), Kwp(Ke), Kwx(Ke), Kwy(Ke), Kwz(Ke),
    Kpu(Ke), Kpv(Ke), Kpw(Ke), Kpp(Ke), Kpx(Ke), Kpy(Ke), Kpz(Ke),
    Kxu(Ke), Kxv(Ke), Kxw(Ke), Kxp(Ke), Kxx(Ke), Kxy(Ke), Kxz(Ke),
    Kyu(Ke), Kyv(Ke), Kyw(Ke), Kyp(Ke), Kyx(Ke), Kyy(Ke), Kyz(Ke),
    Kzu(Ke), Kzv(Ke), Kzw(Ke), Kzp(Ke), Kzx(Ke), Kzy(Ke), Kzz(Ke);
#endif

  DenseSubVector<Number>
    Fu(Fe),
    Fv(Fe),
    Fp(Fe),
    Fx(Fe),
    Fy(Fe);

  #if THREED
  DenseSubVector<Number>
    Fw(Fe),
    Fz(Fe);
  #endif

  // This vector will hold the degree of freedom indices for
  // the element.  These define where in the global system
  // the element degrees of freedom get mapped.
  std::vector<unsigned int> dof_indices;
  std::vector<unsigned int> dof_indices_u;
  std::vector<unsigned int> dof_indices_v;
  std::vector<unsigned int> dof_indices_p;
  std::vector<unsigned int> dof_indices_x;
  std::vector<unsigned int> dof_indices_y;
  #if THREED
  std::vector<unsigned int> dof_indices_w;
  std::vector<unsigned int> dof_indices_z;
  #endif
  
 MeshBase::const_element_iterator       el     = mesh.active_local_elements_begin();
  const MeshBase::const_element_iterator end_el = mesh.active_local_elements_end(); 
  

//vectors needed to store bcs. (wha happened to condense ?)
std::vector<  int > rows;
std::vector<  int > pressure_rows;
std::vector< Real > rows_values;
std::vector< Real > pressure_rows_values;

std::vector<unsigned int> stab_dofs_rows;
std::vector<unsigned int> stab_dofs_cols;
std::vector<Real> stab_dofs_vals;

  for ( ; el != end_el; ++el)
    {    

      const Elem* elem = *el;

      dof_map.dof_indices (elem, dof_indices);
      dof_map.dof_indices (elem, dof_indices_u, u_var);
      dof_map.dof_indices (elem, dof_indices_v, v_var);
      dof_map.dof_indices (elem, dof_indices_p, p_var);
      dof_map.dof_indices (elem, dof_indices_x, x_var);
      dof_map.dof_indices (elem, dof_indices_y, y_var);
      #if THREED
      dof_map.dof_indices (elem, dof_indices_w, w_var);
      dof_map.dof_indices (elem, dof_indices_z, z_var);
      #endif

      const unsigned int n_dofs   = dof_indices.size();
      const unsigned int n_u_dofs = dof_indices_u.size(); 
      const unsigned int n_v_dofs = dof_indices_v.size();
      const unsigned int n_p_dofs = dof_indices_p.size();
      const unsigned int n_x_dofs = dof_indices_x.size(); 
      const unsigned int n_y_dofs = dof_indices_y.size();
      #if THREED
      const unsigned int n_w_dofs = dof_indices_w.size();
      const unsigned int n_z_dofs = dof_indices_z.size();
      #endif
      
      // Compute the element-specific data for the current
      // element.  This involves computing the location of the
      // quadrature points (q_point) and the shape functions
      // (phi, dphi) for the current element.
      fe_disp->reinit  (elem);
      fe_vel->reinit  (elem);
      fe_pres->reinit (elem);

      // Zero the element matrix and right-hand side before
      // summing them.  We use the resize member here because
      // the number of degrees of freedom might have changed from
      // the last element.  Note that this will be the case if the
      // element type is different (i.e. the last element was a
      // triangle, now we are on a quadrilateral).
      Ke.resize (n_dofs, n_dofs);
      Fe.resize (n_dofs);

      Kuu.reposition (u_var*n_u_dofs, u_var*n_u_dofs, n_u_dofs, n_u_dofs);
      Kuv.reposition (u_var*n_u_dofs, v_var*n_u_dofs, n_u_dofs, n_v_dofs);
      Kup.reposition (u_var*n_u_dofs, p_var*n_u_dofs, n_u_dofs, n_p_dofs);
      Kux.reposition (u_var*n_u_dofs, p_var*n_u_dofs + n_p_dofs , n_u_dofs, n_x_dofs);
      Kuy.reposition (u_var*n_u_dofs, p_var*n_u_dofs + n_p_dofs+n_x_dofs , n_u_dofs, n_y_dofs);
      #if THREED
      Kuw.reposition (u_var*n_u_dofs, w_var*n_u_dofs, n_u_dofs, n_w_dofs);
      Kuz.reposition (u_var*n_u_dofs, p_var*n_u_dofs + n_p_dofs+2*n_x_dofs , n_u_dofs, n_z_dofs);
      #endif

      Kvu.reposition (v_var*n_v_dofs, u_var*n_v_dofs, n_v_dofs, n_u_dofs);
      Kvv.reposition (v_var*n_v_dofs, v_var*n_v_dofs, n_v_dofs, n_v_dofs);
      Kvp.reposition (v_var*n_v_dofs, p_var*n_v_dofs, n_v_dofs, n_p_dofs);
      Kvx.reposition (v_var*n_v_dofs, p_var*n_u_dofs + n_p_dofs , n_v_dofs, n_x_dofs);
      Kvy.reposition (v_var*n_v_dofs, p_var*n_u_dofs + n_p_dofs+n_x_dofs , n_v_dofs, n_y_dofs);
      #if THREED
      Kvw.reposition (v_var*n_u_dofs, w_var*n_u_dofs, n_v_dofs, n_w_dofs);
      Kuz.reposition (v_var*n_u_dofs, p_var*n_u_dofs + n_p_dofs+2*n_x_dofs , n_u_dofs, n_z_dofs);
      #endif

      #if THREED
      Kwu.reposition (w_var*n_w_dofs, u_var*n_v_dofs, n_v_dofs, n_u_dofs);
      Kwv.reposition (w_var*n_w_dofs, v_var*n_v_dofs, n_v_dofs, n_v_dofs);
      Kwp.reposition (w_var*n_w_dofs, p_var*n_v_dofs, n_v_dofs, n_p_dofs);
      Kwx.reposition (w_var*n_w_dofs, p_var*n_u_dofs + n_p_dofs , n_v_dofs, n_x_dofs);
      Kwy.reposition (w_var*n_w_dofs, p_var*n_u_dofs + n_p_dofs+n_x_dofs , n_v_dofs, n_y_dofs);
      Kww.reposition (w_var*n_w_dofs, w_var*n_u_dofs, n_v_dofs, n_w_dofs);
      Kwz.reposition (w_var*n_w_dofs, p_var*n_u_dofs + n_p_dofs+2*n_x_dofs , n_u_dofs, n_z_dofs);
      #endif

      Kpu.reposition (p_var*n_u_dofs, u_var*n_u_dofs, n_p_dofs, n_u_dofs);
      Kpv.reposition (p_var*n_u_dofs, v_var*n_u_dofs, n_p_dofs, n_v_dofs);
      Kpp.reposition (p_var*n_u_dofs, p_var*n_u_dofs, n_p_dofs, n_p_dofs);
      Kpx.reposition (p_var*n_v_dofs, p_var*n_u_dofs + n_p_dofs , n_p_dofs, n_x_dofs);
      Kpy.reposition (p_var*n_v_dofs, p_var*n_u_dofs + n_p_dofs+n_x_dofs , n_p_dofs, n_y_dofs);
      #if THREED
      Kpw.reposition (p_var*n_u_dofs, w_var*n_u_dofs, n_p_dofs, n_w_dofs);
      Kpz.reposition (p_var*n_u_dofs, p_var*n_u_dofs + n_p_dofs+2*n_x_dofs , n_p_dofs, n_z_dofs);
      #endif

      Kxu.reposition (p_var*n_u_dofs + n_p_dofs, u_var*n_u_dofs, n_x_dofs, n_u_dofs);
      Kxv.reposition (p_var*n_u_dofs + n_p_dofs, v_var*n_u_dofs, n_x_dofs, n_v_dofs);
      Kxp.reposition (p_var*n_u_dofs + n_p_dofs, p_var*n_u_dofs, n_x_dofs, n_p_dofs);
      Kxx.reposition (p_var*n_u_dofs + n_p_dofs, p_var*n_u_dofs + n_p_dofs , n_x_dofs, n_x_dofs);
      Kxy.reposition (p_var*n_u_dofs + n_p_dofs, p_var*n_u_dofs + n_p_dofs+n_x_dofs , n_x_dofs, n_y_dofs);
      #if THREED
      Kxw.reposition (p_var*n_u_dofs + n_p_dofs, w_var*n_u_dofs, n_x_dofs, n_w_dofs);
      Kxz.reposition (p_var*n_u_dofs + n_p_dofs, p_var*n_u_dofs + n_p_dofs+2*n_x_dofs , n_x_dofs, n_z_dofs);
      #endif


      Kyu.reposition (p_var*n_u_dofs + n_p_dofs+n_x_dofs, u_var*n_u_dofs, n_y_dofs, n_u_dofs);
      Kyv.reposition (p_var*n_u_dofs + n_p_dofs+n_x_dofs, v_var*n_u_dofs, n_y_dofs, n_v_dofs);
      Kyp.reposition (p_var*n_u_dofs + n_p_dofs+n_x_dofs, p_var*n_u_dofs, n_y_dofs, n_p_dofs);
      Kyx.reposition (p_var*n_u_dofs + n_p_dofs+n_x_dofs, p_var*n_u_dofs + n_p_dofs , n_y_dofs, n_x_dofs);
      Kyy.reposition (p_var*n_u_dofs + n_p_dofs+n_x_dofs, p_var*n_u_dofs + n_p_dofs+n_x_dofs , n_y_dofs, n_y_dofs);
      #if THREED
      Kyw.reposition (p_var*n_u_dofs + n_p_dofs+n_x_dofs, w_var*n_u_dofs, n_x_dofs, n_w_dofs);
      Kyz.reposition (p_var*n_u_dofs + n_p_dofs+n_x_dofs, p_var*n_u_dofs + n_p_dofs+2*n_x_dofs , n_x_dofs, n_z_dofs);
      #endif

      #if THREED
      Kzu.reposition (p_var*n_u_dofs + n_p_dofs+2*n_x_dofs, u_var*n_u_dofs, n_y_dofs, n_u_dofs);
      Kzv.reposition (p_var*n_u_dofs + n_p_dofs+2*n_x_dofs, v_var*n_u_dofs, n_y_dofs, n_v_dofs);
      Kzp.reposition (p_var*n_u_dofs + n_p_dofs+2*n_x_dofs, p_var*n_u_dofs, n_y_dofs, n_p_dofs);
      Kzx.reposition (p_var*n_u_dofs + n_p_dofs+2*n_x_dofs, p_var*n_u_dofs + n_p_dofs , n_y_dofs, n_x_dofs);
      Kzy.reposition (p_var*n_u_dofs + n_p_dofs+2*n_x_dofs, p_var*n_u_dofs + n_p_dofs+n_x_dofs , n_y_dofs, n_y_dofs);
      Kzw.reposition (p_var*n_u_dofs + n_p_dofs+2*n_x_dofs, w_var*n_u_dofs, n_x_dofs, n_w_dofs);
      Kzz.reposition (p_var*n_u_dofs + n_p_dofs+2*n_x_dofs, p_var*n_u_dofs + n_p_dofs+2*n_x_dofs , n_x_dofs, n_z_dofs);
      #endif



      Fu.reposition (u_var*n_u_dofs, n_u_dofs);
      Fv.reposition (v_var*n_u_dofs, n_v_dofs);
      Fp.reposition (p_var*n_u_dofs, n_p_dofs);
      Fx.reposition (p_var*n_u_dofs + n_p_dofs, n_x_dofs);
      Fy.reposition (p_var*n_u_dofs + n_p_dofs+n_x_dofs, n_y_dofs);
      #if THREED
      Fw.reposition (w_var*n_u_dofs, n_w_dofs);
      Fz.reposition (p_var*n_u_dofs + n_p_dofs+2*n_x_dofs, n_y_dofs);
      #endif
    
      // Now we will build the element matrix.
      for (unsigned int qp=0; qp<qrule.n_points(); qp++)
        {
        test(7);


        Point div_old_u;
        for (unsigned int l=0; l<n_u_dofs; l++)
        {
          div_old_u(0) += dphi[l][qp](0)*system.old_local_solution->el(dof_indices_u[l]);
          div_old_u(1) += dphi[l][qp](1)*system.old_local_solution->el(dof_indices_v[l]);
          #if THREED
          div_old_u(2) += dphi[l][qp](2)*system.old_local_solution->el(dof_indices_w[l]);
          #endif
        }

            //Elastcity (Mixture) momentum equation



        // Assemble the u-velocity row
          // uu coupling
        Real fac=2;
          for (unsigned int i=0; i<n_u_dofs; i++){
            for (unsigned int j=0; j<n_u_dofs; j++){
              Kuu(i,j) += fac*mu*JxW[qp]*(dphi[i][qp](0)*dphi[j][qp](0));
              Kuu(i,j) += fac*mu*JxW[qp]*(1.0/2.0)*(dphi[i][qp](1)*dphi[j][qp](1));
              Kuu(i,j) += fac*mu*JxW[qp]*(1.0/2.0)*(dphi[i][qp](2)*dphi[j][qp](2));
              Kuv(i,j) += fac*mu*JxW[qp]*(1.0/2.0)*(dphi[i][qp](1)*dphi[j][qp](0));
              #if THREED
              Kuw(i,j) += fac*mu*JxW[qp]*(1.0/2.0)*(dphi[i][qp](2)*dphi[j][qp](0));
              #endif


              Kvv(i,j) += fac*mu*JxW[qp]*(dphi[i][qp](1)*dphi[j][qp](1));
              Kvv(i,j) += fac*mu*JxW[qp]*(1.0/2.0)*(dphi[i][qp](0)*dphi[j][qp](0));
              Kvv(i,j) += fac*mu*JxW[qp]*(1.0/2.0)*(dphi[i][qp](2)*dphi[j][qp](2));
              Kvu(i,j) += fac*mu*JxW[qp]*(1.0/2.0)*(dphi[i][qp](0)*dphi[j][qp](1));
              #if THREED
              Kvw(i,j) += fac*mu*JxW[qp]*(1.0/2.0)*(dphi[i][qp](2)*dphi[j][qp](1));
              #endif

              #if THREED
              Kww(i,j) += fac*mu*JxW[qp]*(dphi[i][qp](2)*dphi[j][qp](2));
              Kww(i,j) += fac*mu*JxW[qp]*(1.0/2.0)*(dphi[i][qp](0)*dphi[j][qp](0));
              Kww(i,j) += fac*mu*JxW[qp]*(1.0/2.0)*(dphi[i][qp](1)*dphi[j][qp](1));
              Kwu(i,j) += fac*mu*JxW[qp]*(1.0/2.0)*(dphi[i][qp](0)*dphi[j][qp](2));
              Kwv(i,j) += fac*mu*JxW[qp]*(1.0/2.0)*(dphi[i][qp](1)*dphi[j][qp](2));
              #endif
        }
    }
    


          // up coupling
          for (unsigned int i=0; i<n_u_dofs; i++)
            for (unsigned int j=0; j<n_p_dofs; j++)
              Kup(i,j) += -JxW[qp]*dphi[i][qp](0)*psi[j][qp];

          // vp coupling
          for (unsigned int i=0; i<n_v_dofs; i++)
            for (unsigned int j=0; j<n_p_dofs; j++)
              Kvp(i,j) += -JxW[qp]*dphi[i][qp](1)*psi[j][qp];

          #if THREED
          // wp coupling
          for (unsigned int i=0; i<n_w_dofs; i++)
            for (unsigned int j=0; j<n_p_dofs; j++)
              Kwp(i,j) += -JxW[qp]*dphi[i][qp](2)*psi[j][qp];
          #endif


            //Divergence term
          for (unsigned int i=0; i<n_u_dofs; i++){
            for (unsigned int j=0; j<n_u_dofs; j++){
              Kuu(i,j) += lambda*JxW[qp]*dphi[i][qp](0)*dphi[j][qp](0);
              Kuv(i,j) += lambda*JxW[qp]*dphi[i][qp](0)*dphi[j][qp](1);
              #if THREED
              Kuw(i,j) += lambda*JxW[qp]*dphi[i][qp](0)*dphi[j][qp](2);
              #endif

              Kvu(i,j) += lambda*JxW[qp]*dphi[i][qp](1)*dphi[j][qp](0);
              Kvv(i,j) += lambda*JxW[qp]*dphi[i][qp](1)*dphi[j][qp](1);
              #if THREED
              Kvw(i,j) += lambda*JxW[qp]*dphi[i][qp](1)*dphi[j][qp](2);
              #endif

              #if THREED
              Kwu(i,j) += lambda*JxW[qp]*dphi[i][qp](2)*dphi[j][qp](0);
              Kwv(i,j) += lambda*JxW[qp]*dphi[i][qp](2)*dphi[j][qp](1);
              Kww(i,j) += lambda*JxW[qp]*dphi[i][qp](2)*dphi[j][qp](2);
              #endif
            }
          }
 

/*  //Mass matrix to get code working if we want to take out elastcicty
					for (unsigned int i=0; i<n_u_dofs; i++){
            for (unsigned int j=0; j<n_u_dofs; j++){
              Kuu(i,j) += JxW[qp]*phi[i][qp]*phi[j][qp];
              Kvv(i,j) += JxW[qp]*phi[i][qp]*phi[j][qp];
     #if THREED
              Kww(i,j) += JxW[qp]*phi[i][qp]*phi[j][qp];
              #endif
            }
          }


          for (unsigned int i=0; i<n_u_dofs; i++){
            for (unsigned int j=0; j<n_u_dofs; j++){
              Kuu(i,j) += JxW[qp]*dphi[i][qp]*dphi[j][qp];
              Kvv(i,j) += JxW[qp]*dphi[i][qp]*dphi[j][qp];
     #if THREED
              Kww(i,j) += JxW[qp]*dphi[i][qp]*dphi[j][qp];
              #endif
            }
          }
*/

          //Mass conservation of mixture
          for (unsigned int i=0; i<n_p_dofs; i++){
            for (unsigned int j=0; j<n_u_dofs; j++){
              Kpu(i,j) += JxW[qp]*psi[i][qp]*dphi[j][qp](0);
            }
          }

          for (unsigned int i=0; i<n_p_dofs; i++){
            for (unsigned int j=0; j<n_v_dofs; j++){
              Kpv(i,j) += JxW[qp]*psi[i][qp]*dphi[j][qp](1);
            }
          }

          #if THREED
          for (unsigned int i=0; i<n_p_dofs; i++){
            for (unsigned int j=0; j<n_w_dofs; j++){
              Kpw(i,j) += JxW[qp]*psi[i][qp]*dphi[j][qp](2);
            }
          }
          #endif


          for (unsigned int i=0; i<n_p_dofs; i++){
            for (unsigned int j=0; j<n_x_dofs; j++){
              Kpx(i,j) += dt*JxW[qp]*psi[i][qp]*f_dphi[j][qp](0);
            }
          }

          for (unsigned int i=0; i<n_p_dofs; i++){
            for (unsigned int j=0; j<n_y_dofs; j++){
              Kpy(i,j) += dt*JxW[qp]*psi[i][qp]*f_dphi[j][qp](1);
            }
          }

          #if THREED
           for (unsigned int i=0; i<n_p_dofs; i++){
            for (unsigned int j=0; j<n_z_dofs; j++){
              Kpz(i,j) += dt*JxW[qp]*psi[i][qp]*f_dphi[j][qp](2);
            }
          }
          #endif




//Source term

          for (unsigned int i=0; i<n_p_dofs; i++){
            #if THREED
            Fp(i) += (div_old_u(0)+div_old_u(1)+div_old_u(2))*JxW[qp]*psi[i][qp];
            #endif

            #if !THREED
            Fp(i) += (div_old_u(0)+div_old_u(1))*JxW[qp]*psi[i][qp];
            #endif
          }

        #if ANAL_2D
					for (unsigned int i=0; i<n_p_dofs; i++){
            Fp(i) += forcing_function_2D(q_point[qp])*JxW[qp]*psi[i][qp];
          }
        #endif



       //Fluid Momentum //Darcy

//std::cout<< (1.0/KPERM) <<std::endl;

Real fluid_fac=(1.0/KPERM);
          //Mass Matrix
          for (unsigned int i=0; i<n_x_dofs; i++)
            for (unsigned int j=0; j<n_x_dofs; j++)
              Kxx(i,j) += fluid_fac*JxW[qp]*(f_phi[i][qp]*f_phi[j][qp]);

          for (unsigned int i=0; i<n_y_dofs; i++)
            for (unsigned int j=0; j<n_y_dofs; j++)
              Kyy(i,j) += fluid_fac*JxW[qp]*(f_phi[i][qp]*f_phi[j][qp]);

          #if THREED
          for (unsigned int i=0; i<n_z_dofs; i++)
            for (unsigned int j=0; j<n_z_dofs; j++)
              Kzz(i,j) += fluid_fac*JxW[qp]*(f_phi[i][qp]*f_phi[j][qp]);
          #endif

          //Grad P
          for (unsigned int i=0; i<n_x_dofs; i++)
            for (unsigned int j=0; j<n_p_dofs; j++)
              Kxp(i,j) += -JxW[qp]*f_dphi[i][qp](0)*psi[j][qp];

          for (unsigned int i=0; i<n_y_dofs; i++)
            for (unsigned int j=0; j<n_p_dofs; j++)
              Kyp(i,j) += -JxW[qp]*f_dphi[i][qp](1)*psi[j][qp];

          #if THREED
          for (unsigned int i=0; i<n_z_dofs; i++)
            for (unsigned int j=0; j<n_p_dofs; j++)
              Kzp(i,j) += -JxW[qp]*f_dphi[i][qp](2)*psi[j][qp];
          #endif
  
} // end qp

	//Pressure jump stabilisation.
#if PRES_STAB  
 	std::vector<unsigned int> stab_dofs_cols2;
	std::vector<Real> stab_dofs_vals2;
    for (unsigned int s=0; s<elem->n_sides(); s++)
    {
      if (elem->neighbor(s) == NULL)
      {   
				//Only do something on the interior edges.
      } //if (elem->neighbor(s) == NULL)
			else{

      const Elem* neighbor = elem->neighbor(s);
   		AutoPtr<Elem> side (elem->build_side(s));


      std::vector<Real> node_co_ords_x;
      std::vector<Real> node_co_ords_y;
      for (unsigned int n=0; n<elem->n_nodes(); n++)
      {
          Node *e_node = elem->get_node(n);
          for (unsigned int m=0; m<neighbor->n_nodes(); m++)
          {
            Node *n_node = neighbor->get_node(m);
            if( ( (*n_node)(0)==(*e_node)(0) ) && ( (*n_node)(1)==(*e_node)(1) ) ){
              node_co_ords_x.push_back((*n_node)(0));
              node_co_ords_y.push_back((*n_node)(1));
              }
          }
      }
     Real side_length=pow ( pow((node_co_ords_x[0]-node_co_ords_x[1]),2) + pow((node_co_ords_y[0]-node_co_ords_y[1]),2),0.5) ;

			std::vector<unsigned int> neighbor_dof_indices_p;
      dof_map.dof_indices(neighbor, neighbor_dof_indices_p,p_var);
      const unsigned int n_neighbor_dofs_p = neighbor_dof_indices_p.size();
			Real hmax=(*elem).hmax();
      Real hmin=(*elem).hmin();
			//Real vol=(*elem).volume();

			Real delta=dt*PEN_STAB;
      #if !THREED
			Real factor=-delta*(side_length*side_length);
      //Real factor=-delta*(side_length);
      #endif
      
      #if THREED
      Real factor=-delta*(hmax*hmax*hmax);
      #endif

      //perf_log.push("push back");
      stab_dofs_cols2.push_back(dof_indices_p[0]);
      stab_dofs_vals2.push_back(-factor);
      stab_dofs_cols2.push_back(neighbor_dof_indices_p[0]);
      stab_dofs_vals2.push_back(factor);
      //perf_log.pop("push back");
			}
		}

	//Lots of mallocs happen during the first assemble due to the unexpected sparsity pattern;
	//The implicit_neighbor_dof does not seem to work ?
	//perf_log.push("kstab");
  DenseMatrix<Number> Kstab2;
  Kstab2.resize(1, stab_dofs_vals2.size());
 	for (int i=0; i < stab_dofs_vals2.size(); i++) {
	 	Kstab2(0,i)=stab_dofs_vals2[i];
	}
 	std::vector<unsigned int> stab_dofs_rows2;
	stab_dofs_rows2.push_back(dof_indices_p[0]);
	system.matrix->add_matrix(Kstab2,stab_dofs_rows2,stab_dofs_cols2);
  test(4);
 
	//perf_log.pop("kstab");
  #endif 
  //endif PRES_STAB

#if PRES_STAB 
//#include "assemble_stokes_bcs_p1p1p0_anal_sine.cpp"

  #if NITSCHE
  #if THREED
  //#include "nitsche_disp_bcs.cpp"
  //#include "penalty_bcs.cpp"

  //Darcy fluid only
  //#include "nitsche_fluid_cylinder_bcs.cpp"

  #include "classic_fluid_cylinder_bcs.cpp"

  //Classic dirichlet for dispalcement
  #include "classic_disp_cylinder_bcs.cpp"



  #endif

    #if !THREED

  //Darcy fluid only

  //#include "nitsche_fluid_square_bcs.cpp"
  //#include "classic_square_fluid_bcs.cpp"


  //#include "nitsche_disp_square_bcs.cpp"
 // #include "classic_square_disp_bcs.cpp"
  #endif

  #endif

#endif



#if !PRES_STAB 
#include "unconfined_cylinder_bcs.cpp"
#endif

//#include "cube_squash_bcs.cpp"




  system.matrix->add_matrix (Ke, dof_indices);
  system.rhs->add_vector    (Fe, dof_indices);

} // end of element loop
  
    system.matrix->close();
    system.rhs->close();
    system.matrix->zero_rows(rows, 1.0);

    for (int i=0; i < rows.size(); i++) {
      system.rhs->set(rows[i],rows_values[i]);
		 // std::cout<<rows_values[i]<<std::endl;
    }

		system.matrix->close();
    system.rhs->close();

    std::cout<<pressure_rows.size()<<std::endl;
    system.matrix->zero_rows(pressure_rows, 1.0);
    for (int i=0; i < pressure_rows.size(); i++) {
		  //std::cout<<pressure_rows_values[i]<<std::endl;
      system.rhs->set(pressure_rows[i],pressure_rows_values[i]);
    }
    system.matrix->close();
    system.rhs->close();

    std::cout<<"Assemble rhs->l2_norm () "<<system.rhs->l2_norm ()<<std::endl;

  // That's it.
  return;
}


