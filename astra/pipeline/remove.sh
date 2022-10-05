img_path=../img
img_proc=../img-processed

for n in "$@"; do
  rm $img_path/$n.*
  rm $img_proc/$n.{axy,png}
  rm $img_proc/$n-objs.png
done
