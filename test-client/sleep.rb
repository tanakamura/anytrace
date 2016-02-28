
def a
  while true
    sleep 1
    printf "%d\n", Process.pid
  end
end

a