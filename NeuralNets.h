typedef enum {
	Delta_Rule
} neural_network_type;

// Original work on Nueral Networks by DrLabman
typedef struct neuralnetwork_s {
	float *inputs;			//input values
	float *inputWeights;	//number input weights = (inputs + 1) * inputs
	float *hiddens;			//values for input to hidden units
	float *hiddenWeights;	//used in a 2 layer network (second layers weights) = (hidden + 1) * hidden
	float *hiddenError;		//error delta for hidden layers
	float *outputs;			//output values, gained from running the inputs through the network
	float *outputError;		//error delta for output
	float *expectedOutputs;	//expected output values, used in learning
	float learningRate;		//rate that learning should effect the network weights
	int numOfInputs;
	int numOfHiddens;
	int numOfOutputs;
	int free;
	neural_network_type type;
} neuralnetwork_t;

//defualt to 128 networks (a quarter of max) 128 is alot of networks anyway
//lots of computations in NN's
#define MIN_NEURAL_NETWORKS 16
#define MAX_NEURAL_NETWORKS 512

void NN_Init();
void NN_Deinit();
int NN_Add(int inputs, int hidden, int outputs);
void NN_Remove(int ident);
void NN_SetInputs(int ident, int start, int n, float *inputs);
void NN_SetInputN(int ident, int n, float input);
void NN_Run(int ident);
float NN_GetOutputN(int ident, int n);
void NN_GetOutputs(int ident, int start, int n, float *outputs);
void NN_SetExpectedOutput(int ident, int n, float expected);
void NN_SetExpectedOutputs(int ident, int start, int n, float *expectedOutputs);
void NN_Learn(int ident, int type);
void NN_SetLearningRate(int ident, float rate);
