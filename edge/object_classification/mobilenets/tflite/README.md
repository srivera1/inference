# MobileNet via TensorFlow Lite (TFLite)

1. [Installation instructions](#installation)
2. [Benchmarking instructions](#benchmarking)
3. [Reference accuracy](#accuracy)
4. [Further information](#further-info)


<a name="installation"></a>
## Installation instructions

Please follow the [common installation instructions](../README.md#installation) first.

### Install TFLite

Install TFLite v1.13.1 from source:
```
$ ck install package --tags=lib,tflite,v1.13.1,vsrc [--target_os=android23-arm64]
```

You can also install TFLite v0.1.7 from a prebuilt binary package for your target e.g.:
```
$ ck list package:lib-tflite-prebuilt*
lib-tflite-prebuilt-0.1.7-linux-aarch64
lib-tflite-prebuilt-0.1.7-linux-x64
lib-tflite-prebuilt-0.1.7-android-arm64
$ ck install package:lib-tflite-prebuilt-0.1.7-android-arm64 [--target_os=android23-arm64]
```

**NB:** Please [let us know](info@dividiti.com) if you would like us to create
prebuilt packages for TFLite 1.13.1.


### Install the MobileNet models for TFLite

To select interactively from one of the non-quantized and quantized
MobileNets-v1-1.0-224 models adopted for MLPerf Inference v0.5:
```
$ ck install package --tags=model,tflite,mlperf,mobilenet
```

To install the [non-quantized model](https://zenodo.org/record/2269307/files/mobilenet_v1_1.0_224.tgz) directly:
```
$ ck install package --tags=model,tflite,mlperf,mobilenet,non-quantized
```

To install the [quantized model](http://download.tensorflow.org/models/mobilenet_v1_2018_08_02/mobilenet_v1_1.0_224_quant.tgz) directly:
```
$ ck install package --tags=model,tflite,mlperf,mobilenet,quantized
```

#### Bonus

##### Install the ResNet model

To install the ResNet50-v1.5 model:
```bash
$ ck install package --tags=model,tflite,mlperf,resnet
More than one package or version found:

 0) model-tflite-resnet50-mlperf-convert-from-tf  Version 1.5  (35e84375ac48dcb1)
 1) model-tflite-resnet50-mlperf  Version 1.5  (d60d4e9a84151271)
```
Option 0 downloads the TF model and converts it to TFLite. Option 1 uses a pre-converted TFLite model.

You can benchmark ResNet exactly in the same way as MobileNet.
Just replace `mobilenet` with `resnet` in the [benchmarking instructions](#benchmarking) below.

##### Install other MobileNets models
You can also install any other MobileNets model compatible with TFLite as follows:
```
$ ck install package --tags=tensorflowmodel,mobilenet,tflite
```

### Compile the TFLite Image Classification client
```
$ ck compile program:image-classification-tflite [--target_os=android23-arm64]
```

### Run the TFLite Image Classification client

Run the client (if required, connect an Android device to your host machine via USB):

- with the non-quantized model:
```
$ ck run program:image-classification-tflite [--target_os=android23-arm64]
...
*** Dependency 3 = weights (TensorFlow-Python model and weights):
...
    Resolved. CK environment UID = 4edbb2648a48d94d (version 1_1.0_224_2018_08_02)
...
ILSVRC2012_val_00000001.JPEG - (65) n01751748 sea snake
0.84 - (65) n01751748 sea snake
0.08 - (58) n01737021 water snake
0.04 - (34) n01665541 leatherback turtle, leatherback, leather...
0.01 - (54) n01729322 hognose snake, puff adder, sand viper
0.01 - (57) n01735189 garter snake, grass snake
---------------------------------------

Summary:
-------------------------------
Graph loaded in 0.000860s
All images loaded in 0.007685s
All images classified in 0.173653s
Average classification time: 0.173653s
Accuracy top 1: 1.0 (1 of 1)
Accuracy top 5: 1.0 (1 of 1)
--------------------------------
```

- with the quantized model:
```
$ ck run program:image-classification-tflite [--target_os=android23-arm64]
...
*** Dependency 3 = weights (TensorFlow-Python model and weights):
...
    Resolved. CK environment UID = 3f0ca5c4d25b4ea3 (version 1_1.0_224_quant_2018_08_02)
...
ILSVRC2012_val_00000001.JPEG - (65) n01751748 sea snake
0.80 - (65) n01751748 sea snake
0.09 - (34) n01665541 leatherback turtle, leatherback, leather...
0.05 - (58) n01737021 water snake
0.03 - (54) n01729322 hognose snake, puff adder, sand viper
0.00 - (33) n01664065 loggerhead, loggerhead turtle, Caretta c...
---------------------------------------

Summary:
-------------------------------
Graph loaded in 0.000589s
All images loaded in 0.000290s
All images classified in 0.450257s
Average classification time: 0.450257s
Accuracy top 1: 1.0 (1 of 1)
Accuracy top 5: 1.0 (1 of 1)
--------------------------------
```

<a name="benchmarking"></a>
## Benchmarking instructions

### Benchmark the performance
```
$ ck benchmark program:image-classification-tflite \
--repetitions=10 --env.CK_BATCH_SIZE=1 --env.CK_BATCH_COUNT=2 \
--record --record_repo=local --record_uoa=mlperf-image-classification-mobilenet-tflite-performance \
--tags=mlperf,image-classification,mobilenet,tflite,performance \
--skip_print_timers --skip_stat_analysis --process_multi_keys
```

**NB:** When using the batch count of **N**, the program classifies **N** images, but
the slow first run is not taken into account when computing the average
classification time e.g.:
```
$ ck benchmark program:image-classification-tflite \
--repetitions=10 --env.CK_BATCH_SIZE=1 --env.CK_BATCH_COUNT=2
...
Processing batches...

Batch 1 of 2

Batch loaded in 0.00802251 s
Batch classified in 0.16831 s

Batch 2 of 2

Batch loaded in 0.00776105 s
Batch classified in 0.0762354 s
...
Summary:
-------------------------------
Graph loaded in 0.000663s
All images loaded in 0.015784s
All images classified in 0.244545s
Average classification time: 0.076235s
Accuracy top 1: 0.5 (1 of 2)
Accuracy top 5: 1.0 (2 of 2)
--------------------------------
```

### Benchmark the accuracy
```
$ ck benchmark program:image-classification-tflite \
--repetitions=1  --env.CK_BATCH_SIZE=1 --env.CK_BATCH_COUNT=50000 \
--record --record_repo=local --record_uoa=mlperf-image-classification-mobilenet-tflite-accuracy \
--tags=mlperf,image-classification,mobilenet,tflite,accuracy \
--skip_print_timers --skip_stat_analysis --process_multi_keys
```


<a name="accuracy"></a>
## Reference accuracy
**TODO**


<a name="further-info"></a>
## Further information
### Using Collective Knowledge
See the [common instructions](../README.md) for information on how to use Collective Knowledge
to learn about [the anatomy of a benchmark](../README.md#anatomy), or
to inspect and visualize [experimental results](../README.md#results).

### Using the client program
See [`ck-tensorflow:program:image-classification-tflite`](https://github.com/ctuning/ck-tensorflow/tree/master/program/image-classification-tflite) for more details about the client program.
