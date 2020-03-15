inotifywait -e close_write,moved_to,create -m code/ |
while read -r directory events filename; do
  if [[ "$filename" == *.cpp ]]; then
    make run
  fi
done
