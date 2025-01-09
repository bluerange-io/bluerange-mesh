# Run the simulator in a docker container

## Setup

### Building the image
Simply run ```docker compose build``` to build the docker image.

### Running the container
Run ```docker compose up -d```. Please make sure that port 8000 is free. Alternatively, configure a different port in the docker compose file.

## How to use the image
After you've built the image and started the container, you can access the server in your local browser with [0.0.0.0:8000](0.0.0.0:8000). However, an error will appear telling you that an exception was thrown. Further investigation shows that your brower is blocking ```SharedArrayBuffer``` for security reasons.

To solve this issue, you can try to host the server (container) with ssl encryption active. This should solve the issue but I haven't tried this. The easier solution is run chrome with SharedArrayBuffer enabled. 

On linux, install Google chrome first and then run
```bash
google-chrome --enable-features=SharedArrayBuffer
```

Now, you can access again [0.0.0.0:8000](0.0.0.0:8000) and the simulation should start.
