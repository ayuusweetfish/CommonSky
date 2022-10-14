for i in $(find ../img-processed -name "*.png" | grep -v -e "-indx.png" -e "-ngc.png" -e "-objs.png")
do
  j=$(basename $i)
  if [ ! -f "${i%.png}.tried" ]; then echo $j; fi
done
