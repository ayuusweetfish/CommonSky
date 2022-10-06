img_proc=../img-processed

for i in `find $img_proc -maxdepth 1 -name *.png`; do
  base=${i%.png}
  if [ -f "$base.tried" ] && [ ! -f "$base.solved" ]; then
    echo `basename $base`
  fi
done
