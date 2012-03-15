/*********************************/
/* Neural Networks Code          */
/*                               */
/* Code by DrLabman              */
/*********************************/

#include "quakedef.h"
#include "NeuralNets.h"

int neural_nets;
neuralnetwork_t	*NN;

/*==========
RandomWeight
------------
This function only returns a random number in the range (-1.0) - (1.0) which is used as the initial value of a weights
===========*/
float RandomWeight (void)
{
	return (rand ()&0x7ffe) / ((float)0x3fff) - 1.0f;
}


/*==========
NN_init
------------
Sets up neural network code:

Allocates memory for list of neural networks
===========*/
void NN_Init(){
	int i;

	//check the command line to see if a number of neural networks was given
	i = COM_CheckParm ("-maxnumberofnns");
	if (i){
		//make sure passed value is between min and max
		neural_nets = bound(MIN_NEURAL_NETWORKS, (int)(Q_atoi(com_argv[i+1])), MAX_NEURAL_NETWORKS);
	} else {
		neural_nets = MAX_NEURAL_NETWORKS / 4;					//defualt to a quarter of max
	}

	//allocate memory for list of nets
	NN = (neuralnetwork_t *)Hunk_AllocName (neural_nets * sizeof(neuralnetwork_t), "neuralnets");

	//label each spot as being a free network
	for (i=0; i<neural_nets; i++){
		NN[i].free = true;
	}
}

/*==========
NN_deinit
------------
clean up on isle 4

Get rid of any neural nets hanging.
============*/
void NN_Deinit(){
	int i;

	//if the net isnt free remove it
	for (i=0; i<neural_nets; i++){
		if (NN[i].free != true){
			NN_Remove(i);
		}
	}	
}

/*==========
NN_add
------------
creates a neural network and returns an identifier to it
============*/
int NN_Add(int inputs, int hidden, int outputs){
	int		ident, i;
	neuralnetwork_t	*temp;

	//do Neural Networks need to survive over map changes?

	//find first free network slot
	for (ident = 0; ident < neural_nets; ident++){
		if (NN[ident].free){
			break;	//this one be free
		}
	}

	//we actually have a free one
	if (ident == neural_nets){
		Con_Print("Ran out of space for neural networks. Use a bigger -maxnumberofnns.");
		return -1;	//return error
	}

	//allocate a neural net in free space
	temp = &NN[ident];
	temp->free = false;	//now we be using it

	//store in quakes memory?
	//convert to one malloc?
	//allocate all arrays
	temp->inputs = (float *)malloc(sizeof(float)*(inputs+1));	//one extra for a bias weight
	temp->inputWeights = (float *)malloc(sizeof(float)*((inputs+1)*(inputs+1)));	//one extra for a bias weight
	temp->hiddens = (float *)malloc(sizeof(float)*(hidden+1));
	temp->hiddenWeights = (float *)malloc(sizeof(float)*((hidden+1)*(hidden+1)));
	temp->hiddenError = (float *)malloc(sizeof(float)*(hidden));
	temp->outputs = (float *)malloc(sizeof(float)*(outputs));
	temp->outputError = (float *)malloc(sizeof(float)*(outputs));
	temp->expectedOutputs = (float *)malloc(sizeof(float)*(outputs));

	//setup numbers
	temp->numOfInputs = inputs;
	temp->numOfHiddens = hidden;
	temp->numOfOutputs = outputs;

	temp->learningRate = 0.2f;	//default to smaller learning rate

	//setup bias
	temp->inputs[inputs] = 1.0f;
	temp->hiddens[hidden] = 1.0f;
    
	//randomise weights
	for (i=0; i<=inputs; i++){
		temp->inputWeights[i] = RandomWeight();
	}

	for (i=0; i<=hidden; i++){
		temp->hiddenWeights[i] = RandomWeight();
	}

	return ident;	//this is the ident to use this NN
}

/*==========
NN_remove
------------
removes a neural network from memory
============*/
void NN_Remove(int ident){
	neuralnetwork_t	*temp;

	temp = &NN[ident];

	//deallocate all arrays
	free(temp->inputs);
	free(temp->inputWeights);
	free(temp->hiddens);
	free(temp->hiddenWeights);
	free(temp->outputs);
	free(temp->expectedOutputs);

	//we are free!
	temp->free = true;
}

/*==========
NN_SetInputs
------------
accepts an array of floats and copys it to the set of inputs for the neural network
from input 'start' to input 'start + n'
============*/
void NN_SetInputs(int ident, int start, int n, float *inputs){
	int i, end = start + n;
	neuralnetwork_t	*temp;

	temp = &NN[ident];

	if (start >= 0 && end <= temp->numOfInputs){
		for(i=start; i<end; i++){
			temp->inputs[i] = inputs[i-start];
		}
	} else {
		Con_DPrintf("Neural Network: Tried to copy array into invalid inputs from: %i to: %i when max of inputs for net was: %i", start, start + n, temp->numOfInputs);
	}
}

/*==========
NN_SetInputN
------------
Set input number 'n' to be value of 'input'
============*/
void NN_SetInputN(int ident, int n, float input){
	if (n<NN[ident].numOfInputs && n>=0){
		NN[ident].inputs[n] = input;
	} else {
		Con_DPrintf("Neural Network: Tried to access invalid input number: %i with max of inputs for net was: %i", n, NN[ident].numOfInputs);
	}
}

/*==========
NN_Run
------------
Runs Neural Network using current inputs and works out output
============*/
void NN_Run(int ident){
	int i,j,num_units,num_weights;
	neuralnetwork_t	*temp;
	float *from, *weights, *to;

	temp = &NN[ident];

	//add up input * weight for all input neurons
	//put values into outputs for single layer networks

	if (temp->numOfHiddens != 0){
		//if we have a hidden layer then work out the output of the hidden layer
		to = &temp->hiddens[0];
		from = &temp->inputs[0];
		weights = &temp->inputWeights[0];
		num_units = temp->numOfHiddens;
		num_weights = temp->numOfInputs;

		//for each hidden unit
		for (i=0; i<num_units; i++){
			to[i] = 0.0f;	//reset to 0
			//add up total weighting of inputs
			for (j=0; j<=num_weights; j++){
				to[i] += from[j] * weights[j];
			}
			//FIX ME: Is this the best activation function
			to[1] = tanh(to[i]);
		}	

		//setup to calculate the output from the hidden layer to the output layer
		to = &temp->outputs[0];
		from = &temp->hiddens[0];
		weights = &temp->hiddenWeights[0];
		num_units = temp->numOfOutputs;
		num_weights = temp->numOfHiddens;

	} else {
		//setup for single layer network from the input stright to the output layer
		to = &temp->outputs[0];
		from = &temp->inputs[0];
		weights = &temp->inputWeights[0];
		num_units = temp->numOfOutputs;
		num_weights = temp->numOfInputs;
	}

	//for each output unit
	for (i=0; i<num_units; i++){
		to[i] = 0.0f;	//reset to 0
		//add up total weighting of inputs
		for (j=0; i<=num_weights; j++){
			to[i] += from[j] * weights[j];
		}
		//FIX ME: Is this the best activation function
		to[1] = tanh(to[i]);
	}
}

/*==========
NN_GetOutputN
------------
get value of output 'n' from Neural Network 'ident'
============*/
float NN_GetOutputN(int ident, int n){
	if (n<NN[ident].numOfOutputs && n>=0){
		return NN[ident].outputs[n];
	} else {
		return 0;
		Con_DPrintf("Neural Network: Tried to access invalid output number: %i", n);
	}
}

/*==========
NN_GetOutputs
------------
Get array of outputs starting at 'start' to output 'start + n'
and return in float array provided
============*/
void NN_GetOutputs(int ident, int start, int n, float *outputs){
	int i, end = start + n;
	neuralnetwork_t	*temp;

	temp = &NN[ident];

	if (start>=0 && end<=temp->numOfOutputs){
		for(i=start; i<end; i++){
			outputs[i-start] = temp->outputs[i];
		}
	} else {
		Con_DPrintf("Neural Network: Tried to copy array from invalid outputs from: %i to: %i when max of outputs for net was: %i", start, start + n, temp->numOfOutputs);
	}
}

/*==========
NN_SetExpectedOutputs
------------
Set expected output 'n' to be value passed in 'expected' for a Neural Network 'ident'
============*/
void NN_SetExpectedOutput(int ident, int n, float expected){
	if (n<NN[ident].numOfOutputs){
		NN[ident].expectedOutputs[n] = expected;
	} else {
		Con_DPrintf("Neural Network: Tried to access invalid expected output");
	}
}

/*==========
NN_SetExpectedOutputs
------------
accepts an array of floats and copys it to the set of expected outputs for the neural network
from exptectedOutput 'start' to expectedOutput 'start + n'
============*/
void NN_SetExpectedOutputs(int ident, int start, int n, float *expectedOutputs){
	int i, end = start + n;
	neuralnetwork_t	*temp;

	temp = &NN[ident];

	if (start >= 0 && end <= temp->numOfInputs){
		for(i=start; i<end; i++){
			temp->expectedOutputs[i] = expectedOutputs[i-start];
		}
	} else {
		Con_DPrintf("Neural Network: Tried to copy array into invalid expectedOutputs from: %i to: %i when max of expectedOutputs for net was: %i", start, start + n, temp->numOfInputs);
	}
}

/*==========
NN_Learn
------------
run training selected training algo on a Neural Network(currently only supporting delta rule learning (backprop)
============*/
void NN_Learn(int ident, int type){
	//ajust weights using detla rule
	neuralnetwork_t *temp;
	int i,j;
	float *desired, *actual, *input, *weight, *error;
	float learning;
//	float delta;

	temp = &NN[ident];

	learning = temp->learningRate;

	//work out error of each output
	desired = &temp->expectedOutputs[0];
	actual = &temp->outputs[0];
	error = &temp->outputError[0];
	input = &temp->inputs[0];
	weight = &temp->inputWeights[0];
	

	//for each of the outputs
	for (i=0; i<temp->numOfOutputs; i++){
		error[i] = learning * (desired[i]-actual[i]);
		for (j=0; j<temp->numOfInputs; j++){
			weight[j] += error[i] * input[j];
		}
	}
/*
	//compute the gradient in the units of the first layer
	for (i=0; i<temp->numOfOutputs; i++){
		error[j] = atanh(net_sum) * (desired[j] - output[j])
	}

	//process the layers backwards and propagate the error gradient
	for each layer from last-1 down to first
		for each unit j in layer
			total = 0
			//add up the weighted error gradient from the next layer
			for each unit k in layer+1
				total +- delta[k] * weights[j][k]
			end for
			delta[j] = atanh(net_sum) * total
		end for
	end for

	if (temp->numOfHiddens != 0){
		//FIXME!
		//HIDDEN UNITS CURRENTLY NOT SUPPORTED
/*
		//reset hidden layer error sum
		for (i=0; i<temp->numOfHiddens; i++){
			temp->hiddenError[i] = 0;
		}

		//for each unit
		desired = &temp->expectedOutputs[0];
		actual = &temp->outputs[0];
		input = &temp->hiddens[0];
		weight = &temp->hiddenWeights[0];
		error = &temp->hiddenError[0];

		for (i=0; i<temp->numOfHiddens; i++){
			float delta = learning * (desired[i]-actual[i]);
			for (j=0; j<temp->numOfInputs; j++){
				weight[j] += delta * input[j];
				error[j] += delta * input[j];
			}
		}

		//for each unit
		desired = &temp->hiddelErrors[0];
		actual = &temp->outputs[0];
		input = &temp->inputs[0];
		weight = &temp->inputWeights[0];
	}
	*/
}

/*==========
NN_SetLearningRate
------------
Sets the learning rate for Neural Network 'ident', used by some training algos (eg. backpropergation)
============*/
void NN_SetLearningRate(int ident, float rate){
	NN[ident].learningRate = rate;
}