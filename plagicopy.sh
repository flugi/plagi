OIFS=$IFS
IFS="
"
PW=$1
for i in $({ cd $PW && find . -type f;}) ; do 
FROM=$(echo $PW/$i | sed "s/\.\///g" )
TO=$(echo $i | sed "s/\.\///g" | sed "s/\//_/g" | sed "s/ //g")
cp $FROM $TO
done
IFS=$OIFS