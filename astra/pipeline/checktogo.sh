for i in $(find ../img-processed -name "*.png" -maxdepth 1 | grep -v -e "-indx.png" -e "-ngc.png" -e "-objs.png" -e "-mod.png")
do
  j=$(basename $i)
  if [ ! -f "${i%.png}.tried" ]; then echo $j; fi
done
