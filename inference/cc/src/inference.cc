#include <tensorflow/core/protobuf/meta_graph.pb.h>
#include <tensorflow/core/public/session.h>
#include <tensorflow/core/public/session_options.h>
#include <tensorflow/c/c_api.h>
#include <iostream>
#include <string>
#include <cstdlib>
#include "Input.h"

#include <cstdio>
#include <sys/time.h>


typedef vector<pair<string, tensorflow::Tensor>> tensor_dict;

void LoadLSTMLibrary() {
  // Load _lstm_ops.so library(from LB_LIBRARY_PATH) for LSTMBlockFusedCell()
  TF_Status* status = TF_NewStatus();
  TF_LoadLibrary("_lstm_ops.so", status);
  if( TF_GetCode(status) != TF_OK ) {
    throw runtime_error("fail to load _lstm_ops.so");
  }
  TF_DeleteStatus(status);
}

tensorflow::Status LoadModel(tensorflow::Session *sess, string graph_fn,
                             string checkpoint_fn = "") {
  tensorflow::Status status;

  // Read in the protobuf graph we exported
  tensorflow::MetaGraphDef graph_def;
  status = ReadBinaryProto(tensorflow::Env::Default(), graph_fn, &graph_def);
  if( status != tensorflow::Status::OK() ) return status;

  // Create the graph in the current session
  status = sess->Create(graph_def.graph_def());
  if( status != tensorflow::Status::OK() ) return status;

  // Restore model from checkpoint, iff checkpoint is given
  if( checkpoint_fn != "" ) {
    const string restore_op_name = graph_def.saver_def().restore_op_name();
    const string filename_tensor_name =
        graph_def.saver_def().filename_tensor_name();

    tensorflow::Tensor filename_tensor(tensorflow::DT_STRING,
                                       tensorflow::TensorShape());
    filename_tensor.scalar<string>()() = checkpoint_fn;

    tensor_dict feed_dict = {{filename_tensor_name, filename_tensor}};
    status = sess->Run(feed_dict, {}, {restore_op_name}, nullptr);
    if( status != tensorflow::Status::OK() ) return status;
  } else {
    // virtual Status Run(const vector<pair<string, Tensor> >& inputs,
    //                  const vector<string>& output_tensor_names,
    //                  const vector<string>& target_node_names,
    //                  vector<Tensor>* outputs) = 0;
    status = sess->Run({}, {}, {"init"}, nullptr);
    if( status != tensorflow::Status::OK() ) return status;
  }

  return tensorflow::Status::OK();
}

tensorflow::Status LoadFrozenModel(tensorflow::Session *sess, string graph_fn) {
  tensorflow::Status status;

  // Read in the protobuf graph we exported
  tensorflow::GraphDef graph_def;
  status = ReadBinaryProto(tensorflow::Env::Default(), graph_fn, &graph_def);
  if( status != tensorflow::Status::OK() ) return status;

  // Create the graph in the current session
  status = sess->Create(graph_def);
  if( status != tensorflow::Status::OK() ) return status;

  return tensorflow::Status::OK();
}

int main(int argc, char const *argv[]) {

  if( argc < 3 ) {
    cerr << argv[0] << " <frozen_graph_fn> <vocab_fn>" << endl;
    return 1;
  } 

  const string frozen_graph_fn = argv[1];
  const string vocab_fn = argv[2];

  // Prepare session
  tensorflow::Session *sess;
  tensorflow::SessionOptions options;
  TF_CHECK_OK(tensorflow::NewSession(options, &sess));
  LoadLSTMLibrary();
  TF_CHECK_OK(LoadFrozenModel(sess, frozen_graph_fn));

  // Prepare config, vocab, input
  Config config = Config(300, 15, true); // wrd_dim=300, word_length=15, use_crf=true
  Vocab vocab = Vocab(vocab_fn, false);  // lowercase=false
  // set class_size to config
  config.SetClassSize(vocab.GetTagVocabSize());
  cerr << "class size = " << config.GetClassSize() << endl;

  struct timeval t1,t2,t3,t4;
  int num_buckets = 0;
  double total_duration_time = 0.0;
  gettimeofday(&t1, NULL);

  vector<string> bucket;
  for( string line; getline(cin, line); ) {
    if( line == "" ) {
       gettimeofday(&t3, NULL);

       Input input = Input(config, vocab, bucket);
       int max_sentence_length = input.GetMaxSentenceLength();
       tensorflow::Tensor* sentence_word_ids = input.GetSentenceWordIds();
       tensorflow::Tensor* sentence_wordchr_ids = input.GetSentenceWordChrIds();
       tensorflow::Tensor* sentence_pos_ids = input.GetSentencePosIds();
       tensorflow::Tensor* sentence_etcs = input.GetSentenceEtcs();
       tensorflow::Tensor* sentence_length = input.GetSentenceLength();
       tensorflow::Tensor* is_train = input.GetIsTrain();
#ifdef DEBUG
       cout << "[word ids]" << endl;
       auto data_word_ids = sentence_word_ids->flat<int>().data();
       for( int i = 0; i < max_sentence_length; i++ ) {
         cout << data_word_ids[i] << " ";
       }
       cout << endl;
       cout << "[wordchr ids]" << endl;
       auto data_wordchr_ids = sentence_wordchr_ids->flat<int>().data();
       int word_length = config.GetWordLength();
       for( int i = 0; i < max_sentence_length; i++ ) {
         for( int j = 0; j < word_length; j++ ) {
           cout << data_wordchr_ids[i*word_length + j] << " ";
         }
         cout << endl;
       }
       cout << "[pos ids]" << endl;
       auto data_pos_ids = sentence_pos_ids->flat<int>().data();
       for( int i = 0; i < max_sentence_length; i++ ) {
         cout << data_pos_ids[i] << " ";
       }
       cout << endl;
       cout << "[etcs]" << endl;
       auto data_etcs = sentence_etcs->flat<float>().data();
       int etc_dim = config.GetEtcDim();
       for( int i = 0; i < max_sentence_length; i++ ) {
         for( int j = 0; j < etc_dim; j++ ) {
           cout << data_etcs[i*etc_dim + j] << " ";
         }
         cout << endl;
       }
       cout << "[sentence length]" << endl;
       auto data_sentence_length = sentence_length->flat<int>().data();
       cout << *data_sentence_length << endl;
       cout << "[is_train]" << endl;
       auto data_is_train = is_train->flat<bool>().data();
       cout << *data_is_train << endl;

       cout << endl;
#endif
       tensor_dict feed_dict = {
         {"input_data_word_ids", *sentence_word_ids},
         {"input_data_wordchr_ids", *sentence_wordchr_ids},
         {"input_data_pos_ids", *sentence_pos_ids},
         {"input_data_etcs", *sentence_etcs},
         {"sentence_length", *sentence_length},
         {"is_train", *is_train},
       };
       std::vector<tensorflow::Tensor> outputs;
       TF_CHECK_OK(sess->Run(feed_dict, {"logits", "loss/trans_params", "sentence_lengths"},
                        {}, &outputs));

       cout << "logits           " << outputs[0].DebugString() << endl;
       cout << "trans_params     " << outputs[1].DebugString() << endl;
       cout << "sentence_lengths " << outputs[2].DebugString() << endl;
       num_buckets += 1;
       bucket.clear();

       gettimeofday(&t4, NULL);
       double duration_time = ((t4.tv_sec - t3.tv_sec)*1000000 + t4.tv_usec - t3.tv_usec)/(double)1000000;
       fprintf(stderr,"elapsed time per sentence = %lf sec\n", duration_time);
       total_duration_time += duration_time;
    } else {
       bucket.push_back(line);
    }
  }
  gettimeofday(&t2, NULL);
  double duration_time = ((t2.tv_sec - t1.tv_sec)*1000000 + t2.tv_usec - t1.tv_usec)/(double)1000000;
  fprintf(stderr,"elapsed time = %lf sec\n", duration_time);
  fprintf(stderr,"duration time on average = %lf sec\n", total_duration_time / num_buckets);

  sess->Close();
  return 0;

  // Prepare inputs
  tensorflow::TensorShape data_shape({1, 4});
  tensorflow::Tensor data(tensorflow::DT_FLOAT, data_shape);
  auto data_ = data.flat<float>().data();
  data_[0] = 2;
  data_[1] = 14;
  data_[2] = 33;
  data_[3] = 50;
  tensor_dict feed_dict = {
      {"X", data},
  };

  std::vector<tensorflow::Tensor> outputs;
  TF_CHECK_OK(sess->Run(feed_dict, {"logits"},
                        {}, &outputs));

  std::cout << "input           " << data.DebugString() << std::endl;
  std::cout << "logits          " << outputs[0].DebugString() << std::endl;

  tensorflow::TTypes<float>::Flat logits_flat = outputs[0].flat<float>();
  for( int i = 0; i < 3; i++ ) {
    const float logit = logits_flat(i);
    std::cout << "logit           " << i << "," << logit << std::endl; 
  } 

  return 0;
}