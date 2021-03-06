for (unsigned int s=0; s<elem->n_sides(); s++){
            if (elem->neighbor(s) == NULL)
              {
                AutoPtr<Elem> side (elem->build_side(s));
                              
                for (unsigned int ns=0; ns<side->n_nodes(); ns++)
                  {
                     const Real xf = side->point(ns)(0);
                    const Real yf = side->point(ns)(1);
                    
                    const Real penalty = 1.e10;
                    
                     
                  //  const Real u_value = (xf < .01) ? 1. : 0.;
                    
                 //   const Real v_value = 0.;
                    
                    for (unsigned int n=0; n<elem->n_nodes(); n++)
                      if( (elem->node(n) == side->node(ns) ) && xf<0.01)
                        {

													Real u_value=1;
                          Kxx(n,n) += penalty;
                         // Kvv(n,n) += penalty;
                                      
                          Fx(n) += penalty*u_value;
                         // Fv(n) += penalty*v_value;
                        }
                  } // end face node loop          
              } // end if (elem->neighbor(side) == NULL)
        } // end for loop      
