FROM public.ecr.aws/lambda/python:3.11

# Install build dependencies for C++ and pybind11
RUN yum install -y gcc-c++ make python3-devel && yum clean all

# Install pip requirements
COPY requirements.txt ./
RUN pip install -r requirements.txt

# Copy Alu source code and Python bindings
COPY std/ /var/task/std/
COPY python-binding/ /var/task/python-binding/

# Build and install the Alu native extension
WORKDIR /var/task/python-binding
RUN pip install .

# Copy function code
WORKDIR /var/task
COPY cloud-templates/aws-lambda/app.py ./

# Set the CMD to your handler
CMD ["app.lambda_handler"]
