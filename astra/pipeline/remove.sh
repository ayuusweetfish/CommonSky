img_path=../img
img_proc=../img-processed

n=$1
rm $img_proc/$n.{axy,coeff,corr,match,new,rdls,refi,solved,tried,wcs}
rm $img_proc/$n-{indx.png,indx.xyls,ngc.png,objs.png}
if [ ! -z "$2" ]; then
  rm $img_proc/$n.png
fi
