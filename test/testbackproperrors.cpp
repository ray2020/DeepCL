// Copyright Hugh Perkins 2014,2015 hughperkins at gmail
//
// This Source Code Form is subject to the terms of the Mozilla Public License, 
// v. 2.0. If a copy of the MPL was not distributed with this file, You can 
// obtain one at http://mozilla.org/MPL/2.0/.

#include <iostream>
#include <random>
#include <algorithm>

#include "NeuralNet.h"
#include "BackpropErrorsv2.h"
#include "ActivationFunction.h"
#include "LossLayer.h"

#include "gtest/gtest.h"

#include "test/gtest_supp.h"
#include "test/Sampler.h"
#include "test/WeightRandomizer.h"

using namespace std;

// This file contains tests for calculating errors for the upstream layer

void testNumerically( float learningRate, int batchSize, int imageSize, int filterSize, int numPlanes, ActivationFunction *fn, bool padZeros, int its = 20 ) {
    NeuralNet *net = NeuralNet::maker()->planes(numPlanes)->imageSize(imageSize)->instance();
    net->addLayer( ConvolutionalMaker::instance()->numFilters(1)->filterSize(filterSize)->biased(0)->fn(fn)->padZeros(padZeros) );
    net->addLayer( ConvolutionalMaker::instance()->numFilters(1)->filterSize(filterSize)->biased(0)->fn(fn)->padZeros(padZeros) );
    net->addLayer( SquareLossMaker::instance() );;
    net->setBatchSize( batchSize );

    int inputSize = net->layers[0]->getResultsSize();
    int resultsSize = net->layers[2]->getResultsSize();
    int weightsSize1 = net->layers[1]->getWeightsSize();
    int weightsSize2 = net->layers[2]->getWeightsSize();

    float *inputData = new float[std::max<int>(10000, inputSize )];
    float *expectedResults = new float[std::max<int>(10000, resultsSize )];
    memset( inputData, 0, sizeof(float) * std::max<int>(10000, inputSize ) );
    memset( expectedResults, 0, sizeof(float) * std::max<int>(10000, resultsSize ) );
//    int seed = 0;
    std::mt19937 random = WeightRandomizer::randomize( inputData, std::max<int>(10000, inputSize ), -2.0f, 2.0f );
    WeightRandomizer::randomize( random, expectedResults, std::max<int>(10000, resultsSize ), -2.0f, 2.0f );
    WeightRandomizer::randomize( random, dynamic_cast<ConvolutionalLayer*>(net->layers[1])->weights, weightsSize1, -2.0f, 2.0f );
    dynamic_cast<ConvolutionalLayer*>(net->layers[1])->weightsWrapper->copyToDevice();
    WeightRandomizer::randomize( random, dynamic_cast<ConvolutionalLayer*>(net->layers[2])->weights, weightsSize2, -2.0f, 2.0f );
    dynamic_cast<ConvolutionalLayer*>(net->layers[2])->weightsWrapper->copyToDevice();

    for( int it = 0; it < its; it++ ) {
        float *weightsBefore1 = new float[weightsSize1];
        float *currentWeights = net->layers[1]->getWeights();
        for( int i = 0; i < weightsSize1; i++ ) {
            weightsBefore1[i] = currentWeights[i];
        }
        float *weightsBefore2 = new float[weightsSize2];
        currentWeights = net->layers[2]->getWeights();
        for( int i = 0; i < weightsSize2; i++ ) {
            weightsBefore2[i] = currentWeights[i];
        }

        net->propagate( inputData );
    //    net->print();
        float loss = net->calcLoss(expectedResults);
        dynamic_cast<LossLayer*>(net->layers[3])->calcLoss(expectedResults);
        net->backProp( learningRate, expectedResults );
        dynamic_cast<ConvolutionalLayer*>(net->layers[1])->weightsWrapper->copyToHost();
        // restore 2nd layer weights :-)
        for( int i = 0; i < weightsSize2; i++ ) {
//            dynamic_cast<ConvolutionalLayer*>(net->layers[2])->weights[i] = weightsBefore2[i];
        }
        dynamic_cast<ConvolutionalLayer*>(net->layers[2])->weightsWrapper->copyToDevice();
        net->propagate( inputData );

        float loss2 = net->calcLoss(expectedResults);
        float lossChange = loss - loss2;
        cout << " loss " << loss << " loss2 " << loss2 << " change: " << lossChange << endl;

        float *newWeights = net->layers[1]->getWeights();
        float sumWeightDiff = 0;
        float sumWeightDiffSquared = 0;
        for( int i = 0; i < weightsSize1; i++ ) {
            float diff = newWeights[i] - weightsBefore1[i];
            sumWeightDiff += diff;
            sumWeightDiffSquared += diff * diff;
        }
        newWeights = net->layers[2]->getWeights();
        for( int i = 0; i < weightsSize2; i++ ) {
            float diff = newWeights[i] - weightsBefore2[i];
            sumWeightDiff += diff;
            sumWeightDiffSquared += diff * diff;
        }
        cout << "sumweightsdiff " << sumWeightDiff << endl;
    //    cout << "sumweightsdiff / learningrate " << (sumWeightDiff / learningRate ) << endl;
    //    cout << "sum weightsdiffsquared " << (sumWeightDiffSquared/ learningRate / learningRate * imageSize ) << endl;

        float estimatedLossChangeFromW = sumWeightDiffSquared/ learningRate; // / filterSize;

        cout << " loss change              " << lossChange << endl;
        cout << " estimatedLossChangeFromW " << estimatedLossChangeFromW << endl;
    //    cout << abs(estimatedLossChangeFromW - lossChange ) / lossChange << endl;    
    //    cout << abs(estimatedLossChangeFromW - lossChange ) / estimatedLossChangeFromW << endl;    
        EXPECT_GT( 0.01f * imageSize * imageSize, abs(estimatedLossChangeFromW - lossChange ) / lossChange ); 
        EXPECT_GT( 0.01f * imageSize * imageSize, abs(estimatedLossChangeFromW - lossChange ) / estimatedLossChangeFromW ); 
    }

//    delete[] weights1;
//    delete[] errors;
//    delete[] results;
    delete[] inputData;
}

TEST( testbackproperrors, checknumerically ) {
    float learningRate = 0.1f;
    const int batchSize = 1;
    const int imageSize = 1;
    const int filterSize = 1;
    const int numPlanes = 1;
    bool padZeros = false;

    testNumerically( learningRate, batchSize, imageSize, filterSize, numPlanes, new TanhActivation(), padZeros, 5 );
}

TEST( testbackproperrors, checknumerically_imagesize5_filter3_relu ) {
    float learningRate = 0.0001f;
    const int batchSize = 1;
    const int imageSize = 5;
    const int filterSize = 3;
    const int numPlanes = 1;
    ActivationFunction *fn = new ReluActivation();
    bool padZeros = true;

    testNumerically( learningRate, batchSize, imageSize, filterSize, numPlanes, fn, padZeros );
}

void measurePerf( int instance, int batchSize, LayerDimensions dim, ActivationFunction *fn ) {
    OpenCLHelper *cl = OpenCLHelper::createForFirstGpuOtherwiseCpu();

    int inputSize = dim.inputCubeSize * batchSize;
    int errorsSize = dim.outputCubeSize * batchSize;
    int weightsSize = dim.filtersSize;
    int errorsForUpstreamSize = dim.inputCubeSize * batchSize;
    float *input = new float[inputSize];
    float *errors = new float[errorsSize];
    float *weights = new float[weightsSize];

    WeightRandomizer::randomize( input, inputSize, -0.1f, 0.1f );
    WeightRandomizer::randomize( errors, errorsSize, -0.1f, 0.1f );
    WeightRandomizer::randomize( weights, weightsSize, -0.1f, 0.1f );

    float *errorsForUpstream = new float[errorsForUpstreamSize];
    CLWrapper *inputWrapper = cl->wrap( inputSize, input );
    CLWrapper *errorsWrapper = cl->wrap( errorsSize, errors );
    CLWrapper *weightsWrapper = cl->wrap( weightsSize, weights );
    CLWrapper *errorsForUpstreamWrapper = cl->wrap( errorsForUpstreamSize, errorsForUpstream );
    inputWrapper->copyToDevice();
    errorsWrapper->copyToDevice();
    weightsWrapper->copyToDevice();
    errorsForUpstreamWrapper->createOnDevice();

    StatefulTimer::timeCheck("after init");
    BackpropErrorsv2 *backpropErrorsImpl = BackpropErrorsv2::instanceSpecific( instance, cl, dim, fn );
    for( int it = 0; it < 40; it++ ) {
        backpropErrorsImpl->backpropErrors( batchSize, 
            inputWrapper, errorsWrapper, weightsWrapper,
            errorsForUpstreamWrapper );
    }
    StatefulTimer::timeCheck("after backprop");
    StatefulTimer::dump(true);

    delete errorsForUpstreamWrapper;
    delete weightsWrapper;
    delete inputWrapper;
    delete errorsWrapper;

    delete[] errors;
    delete[] weights;
    delete[] input;
    delete[] errorsForUpstream;

    delete backpropErrorsImpl;
    delete cl;
}

TEST( SLOW_testbackproperrors, perf_kgsgo_32c5 ) {
    int batchSize = 128;
    LayerDimensions dim;
    dim.setInputPlanes( 32 ).setInputImageSize(19).setNumFilters( 32 ).setFilterSize( 5 )
        .setPadZeros( true ).setBiased( true );  
    cout << dim.buildOptionsString() << endl;  
    ActivationFunction *fn = new ReluActivation();

    measurePerf( 2, batchSize, dim, fn );
}

void compareSpecific( int instance0, int instance1, int batchSize, LayerDimensions dim, ActivationFunction *fn ) {
    OpenCLHelper *cl = OpenCLHelper::createForFirstGpuOtherwiseCpu();

    int inputSize = dim.inputCubeSize * batchSize;
    int errorsSize = dim.outputCubeSize * batchSize;
    int weightsSize = dim.filtersSize;
    int errorsForUpstreamSize = dim.inputCubeSize * batchSize;

    float *input = new float[inputSize];
    float *errors = new float[errorsSize];
    float *weights = new float[weightsSize];
    float *errorsForUpstream0 = new float[errorsForUpstreamSize];
    float *errorsForUpstream1 = new float[errorsForUpstreamSize];

    WeightRandomizer::randomize( input, inputSize, -0.1f, 0.1f );
    WeightRandomizer::randomize( errors, errorsSize, -0.1f, 0.1f );
    WeightRandomizer::randomize( weights, weightsSize, -0.1f, 0.1f );

    CLWrapper *inputWrapper = cl->wrap( inputSize, input );
    CLWrapper *errorsWrapper = cl->wrap( errorsSize, errors );
    CLWrapper *weightsWrapper = cl->wrap( weightsSize, weights );
    CLWrapper *errorsForUpstreamWrapper0 = cl->wrap( errorsForUpstreamSize, errorsForUpstream0 );
    CLWrapper *errorsForUpstreamWrapper1 = cl->wrap( errorsForUpstreamSize, errorsForUpstream1 );

    inputWrapper->copyToDevice();
    errorsWrapper->copyToDevice();
    weightsWrapper->copyToDevice();
    errorsForUpstreamWrapper0->createOnDevice();
    errorsForUpstreamWrapper1->createOnDevice();

    BackpropErrorsv2 *bp0 = BackpropErrorsv2::instanceSpecific( instance0, cl, dim, fn );
    BackpropErrorsv2 *bp1 = BackpropErrorsv2::instanceSpecific( instance1, cl, dim, fn );
    
    bp0->backpropErrors( batchSize, 
            inputWrapper, errorsWrapper, weightsWrapper,
            errorsForUpstreamWrapper0 );
    bp1->backpropErrors( batchSize, 
            inputWrapper, errorsWrapper, weightsWrapper,
            errorsForUpstreamWrapper1 );

    errorsForUpstreamWrapper0->copyToHost();
    errorsForUpstreamWrapper1->copyToHost();

    int resultsSize = errorsForUpstreamSize;
    cout << dim << endl;
    bool same = true;
    for( int i = 0; i < max( 20, resultsSize ); i++ ) {
        if( i < resultsSize ) {
            if( abs( errorsForUpstream0[i] - errorsForUpstream1[i] ) < 0.000001 || abs( errorsForUpstream0[i] - errorsForUpstream1[i] ) <= 0.001 * max( abs( errorsForUpstream0[i] ), abs( errorsForUpstream1[i] ) ) ) {
                if( i < 20 ) {
                    cout << "results[" << i << "]=" << errorsForUpstream0[i] << " " << errorsForUpstream1[i];
                    cout << " SAME";
                }
            } else {
                cout << "results[" << i << "]=" << errorsForUpstream0[i] << " " << errorsForUpstream1[i];
                cout << " DIFF";
                same = false;
            }
        } else {
             if( i < 20 ) {
                 cout << "     ";
             }
        }
        if( i < 20 ) {
            cout << "  || " << errorsForUpstream1[100+i] ;
            cout << "  || " << errorsForUpstream1[200+i] ;
            cout << "  || " << errorsForUpstream1[300+i] ;
            cout << "  || " << errorsForUpstream1[400+i] ;
            cout << "  || " << errorsForUpstream1[500+i] ;
            cout << "  || " << errorsForUpstream1[600+i] ;
            cout << "  || " << errorsForUpstream1[700+i] << endl;
        }
    }
    EXPECT_EQ( true, same );

    delete inputWrapper;
    delete errorsWrapper;
    delete weightsWrapper;
    delete errorsForUpstreamWrapper0;
    delete errorsForUpstreamWrapper1;

    delete[] errorsForUpstream0;
    delete[] errorsForUpstream1;
    delete bp0;
    delete bp1;
    delete cl;
    delete[] input;
    delete[] errors;
    delete[] weights;
}

TEST( SLOW_testbackproperrors, compare_kgsgo_32c5 ) {
    int batchSize = 128;
    LayerDimensions dim;
    dim.setInputPlanes( 32 ).setInputImageSize(19).setNumFilters( 32 ).setFilterSize( 5 )
        .setPadZeros( true ).setBiased( true );  
    cout << dim.buildOptionsString() << endl;  
    ActivationFunction *fn = new ReluActivation();

    compareSpecific( 1, 2, batchSize, dim, fn );

}

TEST( SLOW_testbackproperrors, compare_kgsgo_32c5mini ) {
    int batchSize = 4;
    LayerDimensions dim;
    dim.setInputPlanes( 2 ).setInputImageSize(3).setNumFilters( 2 ).setFilterSize( 3 )
        .setPadZeros( true ).setBiased( true );  
    cout << dim.buildOptionsString() << endl;  
    ActivationFunction *fn = new ReluActivation();

    compareSpecific( 1, 2, batchSize, dim, fn );

}

TEST( SLOW_testbackproperrors, compare_kgsgo_32c5mini2 ) {
    int batchSize = 1;
    int imageSize = 2;
    LayerDimensions dim;
    dim.setInputPlanes( 1 ).setInputImageSize(imageSize).setNumFilters( 1 ).setFilterSize( imageSize )
        .setPadZeros( true ).setBiased( true );
    cout << dim.buildOptionsString() << endl;
    ActivationFunction *fn = new ReluActivation();

    compareSpecific( 1, 2, batchSize, dim, fn );

}

/*
float *test( int imageSize ) {
    const int batchSize = 128;
    LayerDimensions dim;
    dim.setInputPlanes( 32 ).setInputImageSize( 28 ).setNumFilters( 32 ).setFilterSize( 5 )
        .setBiased( true ).setPadZeros( true );

    int weightsSize = dim.filtersSize;
    int biasWeightsSize = dim.numFilters;
    int resultsSize = batchSize * dim.outputCubeSize;
    float *weights = new float[max(10000, weightsSize ) ];
    float *biasWeights = new float[max( 10000, biasWeightsSize)];
    float *errors = new float[max(10000, resultsSize )];
    float *results = new float[max(10000, resultsSize )];
    WeightRandomizer::randomize( weights, max(10000, weightsSize ), -1, 1 );
    WeightRandomizer::randomize( biasWeights, max( 10000, biasWeightsSize), -1, 1 );
    WeightRandomizer::randomize( errors, max(10000, resultsSize ), -1, 1 );
    WeightRandomizer::randomize( results, max(10000, resultsSize ), -1, 1 );

    OpenCLHelper cl;
    BackpropErrorsv2 *backpropErrorsImpl = BackpropErrorsv2::instanceForTest( &cl, dim, new ReluActivation() );
    Timer timer;
    float *errorsForUpstream = backpropErrorsImpl->backpropErrors( batchSize, results, weights, biasWeights, errors );
    StatefulTimer::dump(true);
    timer.timeCheck("after calcing errors");

    Sampler::printSamples( "errorsForUpstream", batchSize * dim.inputCubeSize, errorsForUpstream );

    delete backpropErrorsImpl;

    delete[] errors;
    delete[] weights;
    delete[] biasWeights;

    return errorsForUpstream;
}
*/
// we want to test calcerrors for layer 2 in a network like:
//    NeuralNet *net = NeuralNet::maker()->planes(1)->imageSize(28)->instance();
//    net->addLayer( ConvolutionalMaker::instance()->numFilters(32)->filterSize(5)->relu()->biased()->insert();
//    net->addLayer( ConvolutionalMaker::instance()->numFilters(32)->filterSize(5)->relu()->biased()->insert();
//    net->addLayer( ConvolutionalMaker::instance()->numFilters(10)->filterSize(20)->tanh()->biased(config.biased)->insert();
//TEST( testbackproperrors, DISABLED_image28 ) {
//    float *errorsForUpstream = test(28);
//    EXPECT_FLOAT_NEAR( -1.66007, errorsForUpstream[68268] );
//    EXPECT_FLOAT_NEAR( 0.823709, errorsForUpstream[2927151] );
//    EXPECT_FLOAT_NEAR( 6.99365, errorsForUpstream[1746549] );
//    EXPECT_FLOAT_NEAR( 7.25249, errorsForUpstream[576704] );
//    EXPECT_FLOAT_NEAR( 7.88787, errorsForUpstream[570179] );
//    delete[] errorsForUpstream;
//}

//TEST( testbackproperrors, DISABLED_image19 ) { // make it work for a image19 first :-)
//    float *errorsForUpstream = test(19);
//    EXPECT_FLOAT_NEAR( -24.5602, errorsForUpstream[158380] );
//    EXPECT_FLOAT_NEAR( 7.39012, errorsForUpstream[2607] );
//    EXPECT_FLOAT_NEAR( -6.50315, errorsForUpstream[546421] );
//    EXPECT_FLOAT_NEAR( -1.22025, errorsForUpstream[429248] );
//    EXPECT_FLOAT_NEAR( -8.89935, errorsForUpstream[1200963] );
//    delete[] errorsForUpstream;

//    const int batchSize = 128;
//    LayerDimensions dim;
//    dim.setInputPlanes( 32 ).setInputImageSize( 19 ).setNumFilters( 32 ).setFilterSize( 5 )
//        .setBiased( true ).setPadZeros( true );    const int batchSize = 128;
//    LayerDimensions dim;
//    dim.setInputPlanes( 32 ).setInputImageSize( 28 ).setNumFilters( 32 ).setFilterSize( 5 )
//        .setBiased( true ).setPadZeros( true );

//    int weightsSize = dim.filtersSize;
//    int biasWeightsSize = dim.numFilters;
//    int resultsSize = batchSize * dim.outputCubeSize;
//    float *weights = new float[max(10000, weightsSize ) ];
//    float *biasWeights = new float[max( 10000, biasWeightsSize)];
//    float *errors = new float[max(10000, resultsSize )];
//    float *results = new float[max(10000, resultsSize )];
//    WeightRandomizer::randomize( weights, max(10000, weightsSize ), -1, 1 );
//    WeightRandomizer::randomize( biasWeights, max( 10000, biasWeightsSize), -1, 1 );
//    WeightRandomizer::randomize( errors, max(10000, resultsSize ), -1, 1 );
//    WeightRandomizer::randomize( results, max(10000, resultsSize ), -1, 1 );

//    OpenCLHelper cl;
//    BackpropErrors *backpropErrorsImpl = BackpropErrors::instanceForTest( &cl, dim, new ReluActivation() );
//    Timer timer;
//    float *errorsForUpstream = backpropErrorsImpl->backpropErrors( batchSize, results, weights, biasWeights, errors );
//    StatefulTimer::dump(true);
//    timer.timeCheck("after calcing errors");

//    Sampler::printSamples( "errorsForUpstream", batchSize * dim.inputCubeSize, errorsForUpstream );

//    EXPECT_FLOAT_NEAR( -1.66007, errorsForUpstream[68268] );
//    EXPECT_FLOAT_NEAR( 0.823709, errorsForUpstream[2927151] );
//    EXPECT_FLOAT_NEAR( 6.99365, errorsForUpstream[1746549] );
//    EXPECT_FLOAT_NEAR( 7.25249, errorsForUpstream[576704] );
//    EXPECT_FLOAT_NEAR( 7.88787, errorsForUpstream[570179] );

//    delete backpropErrorsImpl;

//    delete[] errorsForUpstream;
//    delete[] errors;
//    delete[] weights;
//    delete[] biasWeights;


//    int weightsSize = dim.filtersSize;
//    int biasWeightsSize = dim.numFilters;
//    int resultsSize = batchSize * dim.outputCubeSize;
//    float *weights = new float[max(10000, weightsSize ) ];
//    float *biasWeights = new float[max( 10000, biasWeightsSize)];
//    float *errors = new float[max(10000, resultsSize )];
//    float *results = new float[max(10000, resultsSize )];
//    WeightRandomizer::randomize( weights, max(10000, weightsSize ), -1, 1 );
//    WeightRandomizer::randomize( biasWeights, max( 10000, biasWeightsSize), -1, 1 );
//    WeightRandomizer::randomize( errors, max(10000, resultsSize ), -1, 1 );
//    WeightRandomizer::randomize( results, max(10000, resultsSize ), -1, 1 );

//    OpenCLHelper cl;
//    BackpropErrors *backpropErrorsImpl = BackpropErrors::instanceForTest( &cl, dim, new ReluActivation() );
//    Timer timer;
//    float *errorsForUpstream = backpropErrorsImpl->backpropErrors( batchSize, results, weights, biasWeights, errors );
//    StatefulTimer::dump(true);
//    timer.timeCheck("after calcing errors");

//    Sampler::printSamples( "errorsForUpstream", batchSize * dim.inputCubeSize, errorsForUpstream );

//    EXPECT_FLOAT_NEAR( -24.5602, errorsForUpstream[158380] );
//    EXPECT_FLOAT_NEAR( 7.39012, errorsForUpstream[2607] );
//    EXPECT_FLOAT_NEAR( -6.50315, errorsForUpstream[546421] );
//    EXPECT_FLOAT_NEAR( -1.22025, errorsForUpstream[429248] );
//    EXPECT_FLOAT_NEAR( -8.89935, errorsForUpstream[1200963] );

//    delete backpropErrorsImpl;

//    delete[] errorsForUpstream;
//    delete[] errors;
//    delete[] weights;
//    delete[] biasWeights;
//}

/*
TEST( testbackproperrors, comparespecific ) {
    const int batchSize = 5;
    LayerDimensions dim;
    dim.setInputPlanes( 1 ).setInputImageSize( 5 ).setNumFilters( 1 ).setFilterSize( 3 )
        .setBiased( true ).setPadZeros( false );

    int weightsSize = dim.filtersSize;
    int biasWeightsSize = dim.numFilters;
    int resultsSize = batchSize * dim.outputCubeSize;
    float *weights = new float[max(10000, weightsSize ) ];
    float *biasWeights = new float[max( 10000, biasWeightsSize)];
    float *errors = new float[max(10000, resultsSize )];
    float *results = new float[max(10000, resultsSize )];
    memset( weights, 0, sizeof(float) * max(10000, weightsSize ) );
    memset( biasWeights, 0, sizeof(float) * max(10000, biasWeightsSize ) );
    memset( errors, 0, sizeof(float) * max(10000, resultsSize ) );
    memset( results, 0, sizeof(float) * max(10000, resultsSize ) );
    mt19937 random = WeightRandomizer::randomize( weights, max(10000, weightsSize ), -1, 1 );
    WeightRandomizer::randomize( random, biasWeights, max( 10000, biasWeightsSize), -1, 1 );
    WeightRandomizer::randomize( random, errors, max(10000, resultsSize ), -1, 1 );
    WeightRandomizer::randomize( random, results, max(10000, resultsSize ), -1, 1 );
//    WeightRandomizer::randomizeInts( weights, max(10000, weightsSize ), 1, 3 );
//    WeightRandomizer::randomizeInts( biasWeights, max( 10000, biasWeightsSize), 0, 3 );
//    WeightRandomizer::randomizeInts( errors, max(10000, resultsSize ), 0, 3 );
//    WeightRandomizer::randomizeInts( results, max(10000, resultsSize ), 0, 3 );

//    weights[0] = 3;
//    weights[1] = 5;
//    weights[2] = 4;

//    weights[25] = 4;
//    weights[49] = 4;

//    weights[50] = 4;
//    weights[99] = 4;

//    weights[75] = 4;
//    weights[99] = 4;

//    weights[100] = 3;
//    weights[124] = 3;

//    errors[0] = 2;
//    errors[1] = 7;
//    errors[2] = 3;
//    errors[3] = 1;
//    errors[4] = 8;
//    errors[5] = 6;

    OpenCLHelper cl;
    BackpropErrorsv2 *backpropErrorsImpl1 = BackpropErrorsv2::instanceSpecific( 0, &cl, dim, new ReluActivation() );
    float *errorsForUpstream1 = backpropErrorsImpl1->backpropErrors( batchSize, results, weights, biasWeights, errors );
    BackpropErrorsv2 *backpropErrorsImpl2 = BackpropErrorsv2::instanceSpecific( 1, &cl, dim, new ReluActivation() );
    float *errorsForUpstream2 = backpropErrorsImpl2->backpropErrors( batchSize, results, weights, biasWeights, errors );

    int errorsForUpstreamSize = batchSize * dim.inputCubeSize;
    cout << dim << endl;
    for( int i = 0; i < 25; i++ ) {
        cout << "results[" << i << "]=" << errorsForUpstream1[i] << " " << errorsForUpstream2[i];
        if( i < resultsSize ) {
            if( errorsForUpstream1[i] == errorsForUpstream2[i] ) {
                cout << " SAME";
            } else {
                cout << " DIFF";
            }
        } else {
            cout << "     ";
        }
        cout << "  || " << errorsForUpstream2[100+i] ;
        cout << "  || " << errorsForUpstream2[200+i] ;
        cout << "  || " << errorsForUpstream2[300+i] ;
        cout << "  || " << errorsForUpstream2[400+i] ;
        cout << "  || " << errorsForUpstream2[500+i] ;
        cout << "  || " << errorsForUpstream2[600+i] ;
        cout << "  || " << errorsForUpstream2[700+i] << endl;
    }
    bool same = true;
    int errCount = 0;
    for( int i = 0; i < errorsForUpstreamSize; i++ ) {
        if( errorsForUpstream1[i] != errorsForUpstream2[i] ) {
            cout << "DIFF: i " << i << " " << errorsForUpstream1[i] << " != " << errorsForUpstream2[i] << endl;
            same = false;
            errCount++;
            if( errCount == 5 ) {
                cout << " ... " << endl;
                break;
            }
        }
    }
    EXPECT_EQ( true, same );

    delete backpropErrorsImpl1;
    delete backpropErrorsImpl2;

    delete[] errorsForUpstream1;
    delete[] errorsForUpstream2;
    delete[] errors;
    delete[] weights;
    delete[] biasWeights;
}
*/

