PROFESSORS CODE HAD A BUG FOR THE ORDER OF THE HOST AND PORT

*** our code only works if the PORT is supplied before the HOST and they have to be the last 2 flags for the client side ***

ex) ./ncTh 9000 localhost <---- this works
    ./ncTh localhost 9000 <---- doesnt work
