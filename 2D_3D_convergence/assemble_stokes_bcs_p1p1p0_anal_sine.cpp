    for (unsigned int s=0; s<elem->n_sides(); s++)
    {
      if (elem->neighbor(s) == NULL)
      {   
        AutoPtr<Elem> side (elem->build_side(s));

        for (unsigned int ns=0; ns<side->n_nodes(); ns++)
        {

          for (unsigned int n=0; n<elem->n_nodes(); n++)
          {
            Node *node = elem->get_node(n);
            const Real xf = (*node)(0);
            Real yf = (*node)(1);
            #if THREED
            Real zf = (*node)(2);
            #endif
            Number value_u = exact_2D_solution_u((*node), es.parameters,"null","void");
            Number value_v = exact_2D_solution_v((*node), es.parameters,"null","void");
						Number value_x = exact_2D_solution_x((*node), es.parameters,"null","void");
            Number value_y = exact_2D_solution_y((*node), es.parameters,"null","void");
            
         

            Number value_p = exact_2D_solution_p((*node), es.parameters,"null","void");
            #if THREED
            Number value_w = exact_2D_solution_w((*node), es.parameters,"null","void");
            Number value_z = exact_2D_solution_z((*node), es.parameters,"null","void");
            #endif

/*
   					std::cout<< "time bcs "<< time <<std::endl;
            std::cout<<"value_x "<< value_x <<std::endl;
            std::cout<<"value_y "<< value_y <<std::endl;
            std::cout<<"value_u "<< value_u <<std::endl;
            std::cout<<"value_v "<< value_v <<std::endl;
            std::cout<<"value_p "<< value_p <<std::endl;
*/

#if !THREED
          if ((elem->node(n) == side->node(ns)) && ( (xf<0.001) || (xf>0.999) || (yf>0.999) || (yf<0.001) ))
#endif 
#if THREED
          if ((elem->node(n) == side->node(ns)) && ( (xf<0.001) || (xf>0.999) || (yf>0.999) || (yf<0.001) || (zf>0.999) || (zf<0.001)))
#endif           
          {
            int source_dof = node->dof_number(system.number(), u_var, 0);
            if((source_dof<12345678) && (source_dof>-1)){
              rows_values.push_back(value_u);
              rows.push_back(source_dof);
            }

            source_dof = node->dof_number(system.number(), v_var, 0);
            if((source_dof<12345678) && (source_dof>-1)){
             rows_values.push_back(value_v);
             rows.push_back(source_dof);
            }

            #if THREED
            source_dof = node->dof_number(system.number(), w_var, 0);
            if((source_dof<12345678) && (source_dof>-1)){
             rows_values.push_back(value_w);
             rows.push_back(source_dof);
            }
            #endif
            
          }  //end if

          #if !THREED
          if ((elem->node(n) == side->node(ns)) && ( (xf<0.001) || (xf>0.999) || (yf>0.999) || (yf<0.001) ))
          #endif 
          #if THREED
          if ((elem->node(n) == side->node(ns)) && ( (xf<0.001) || (xf>0.999) || (yf>0.999) || (yf<0.001) || (zf>0.999) || (zf<0.001)))
          #endif
          {
            
            int source_dof = node->dof_number(system.number(), x_var, 0);
            if((source_dof<12345678) && (source_dof>-1)){
              rows_values.push_back(value_x);
              rows.push_back(source_dof);
            }

             source_dof = node->dof_number(system.number(), y_var, 0);
            if((source_dof<12345678) && (source_dof>-1)){
              rows_values.push_back(value_y);
              rows.push_back(source_dof);
            }

            #if THREED
            source_dof = node->dof_number(system.number(), z_var, 0);
            if((source_dof<12345678) && (source_dof>-1)){
              rows_values.push_back(value_z);
              rows.push_back(source_dof);
            }
            #endif
          }  //end if
          


          #if !THREED
          if ((elem->node(n) == side->node(ns)) && ( (xf<0.001) || (xf>0.999) || (yf>0.999) || (yf<0.001) ))
          #endif 
          #if THREED
          if ((elem->node(n) == side->node(ns)) && ( (xf<0.001) || (xf>0.999) || (yf>0.999) || (yf<0.001) || (zf>0.999) || (zf<0.001)))
          #endif
          {
            int source_dof = elem->dof_number(system.number(), p_var, 0);

					//std::cout<< source_dof <<std::endl;

            if((source_dof<12345678) && (source_dof>-1) ){
             //   pressure_rows_values.push_back(value_p);
             //   pressure_rows.push_back(source_dof);
            }
            
          }  //end if

          }
        }
      } //if (elem->neighbor(s) == NULL)
    } // end boundary condition section  


