solve_field=${solve_field:-solve-field}

if [ ! -f ./selcrop ]; then
  echo "Please build ./selcrop first"
  exit 1
elif [ ! -f ../align/align ]; then
  echo "Please build ../align/align first"
  exit 1
elif ! command $solve_field &>/dev/null; then
  echo "Command \"$solve_field\" is not valid. Please properly set \$solve_field to point to the astrometry.net installation."
  exit 1
fi

img_path=../img
img_proc=../img-processed

images=`find $img_path -name *.jpg`

echo "(1/3) Crop"
for i in $images; do
  bn=`basename $i`
  n=${bn%.*}
  if [ ! -f "$img_proc/$n.png" ]; then
    echo $bn
    ./selcrop $i $img_proc/$n.png || break
  fi
done

echo "(2/3) Solve"
for i in $images; do
  bn=`basename $i`
  n=${bn%.*}
  if [ ! -f "$img_proc/$n.solved" ]; then
    echo $bn
    $solve_field --scale-low 10 --tweak-order 3 --crpix-center $img_proc/$n.png
  fi
done

echo "(3/3) Refine alignment"
for i in $images; do
  bn=`basename $i`
  n=${bn%.*}
  if [ ! -f "$img_proc/$n.coeff" ]; then
    echo $bn
    ../align/align \
      $img_proc/$n.png \
      $img_proc/$n.axy \
      $img_proc/$n.rdls \
      $img_proc/$n-indx.xyls \
      $img_proc/$n.corr \
      $img_proc/$n.wcs \
      $img_proc/$n.refi \
      $img_proc/$n.coeff
  fi
done
