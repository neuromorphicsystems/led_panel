host='pi@192.168.0.14'
path='led_panel'

clang-format -i source/*.hpp source/*.cpp

ssh $host "sudo date -u -s '$(date -u)'"
rsync -avz --exclude build --exclude .git --exclude build ./ $host:$path/
ssh $host "cd $path && make"

