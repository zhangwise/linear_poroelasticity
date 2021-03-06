#!/bin/bash
# My first script

f_prefix="2D_stab"


NE=(8 16 32 64)
NT=(8 16 32 64)
DELTA=(10 100)

#clpc directory
exe_directory="/users/lorenzb/Dphil/libmesh_projetcs/poro_paper_sims/2D_convergence_delta2/"

#loztop directory
#exe_directory="/home/loztop/Dropbox/Dphil/libmesh_projetcs/poro_paper_sims/2D_convergence_delta2/"

matfiles_dir="data/matfiles/"
data_dir="data/"

res_directory_mat="$exe_directory$matfiles_dir"

res_directory_data="$exe_directory$data_dir"


exe_filename="ex11-opt"


str_nt='NT_'
str_ne='NE_'
str_delta='DELTA_'

for k in ${DELTA[@]}
do
for i in ${NE[@]}
do
for j in ${NT[@]}
do




output_file_name_mat="$res_directory_mat$f_prefix"_"$k"_"$str_nt$j"_"$str_ne$i"_.mat" "

output_file_name_data="$res_directory_data$f_prefix"_"$k"_"$str_nt$j"_"$str_ne$i"_" "

exe_str="$exe_directory$exe_filename $j $i $output_file_name_mat $output_file_name_data $k" 
   echo $exe_str

`$exe_str`

done
done
done
