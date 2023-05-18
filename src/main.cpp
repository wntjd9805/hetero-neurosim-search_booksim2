// $Id$

/*
 Copyright (c) 2007-2015, Trustees of The Leland Stanford Junior University
 All rights reserved.

 Redistribution and use in source and binary forms, with or without
 modification, are permitted provided that the following conditions are met:

 Redistributions of source code must retain the above copyright notice, this
 list of conditions and the following disclaimer.
 Redistributions in binary form must reproduce the above copyright notice, this
 list of conditions and the following disclaimer in the documentation and/or
 other materials provided with the distribution.

 THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND
 ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE LIABLE FOR
 ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON
 ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
 SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
*/

/*main.cpp
 *
 *The starting point of the network simulator
 *-Include all network header files
 *-initilize the network
 *-initialize the traffic manager and set it to run
 *
 *
 */
#include <sys/time.h>

#include <cmath>
#include <cstdlib>
#include <fstream>
#include <iostream>
#include <map>
#include <string>

#include "booksim.hpp"
#include "booksim_config.hpp"
#include "injection.hpp"
#include "misc_utils.hpp"
#include "misc_utils.hpp"
#include "network.hpp"
#include "power_module.hpp"
#include "random_utils.hpp"
#include "routefunc.hpp"
#include "traffic.hpp"
#include "trafficmanager.hpp"
#include <sstream>

using namespace std;
///////////////////////////////////////////////////////////////////////////////
// Global declarations
//////////////////////

/* the current traffic manager instance */
TrafficManager *trafficManager = NULL;

int GetSimTime() { return trafficManager->getTime(); }

class Stats;
Stats *GetStats(const std::string &name) {
  Stats *test = trafficManager->getStats(name);
  if (test == 0) {
    cout << "warning statistics " << name << " not found" << endl;
  }
  return test;
}

/* printing activity factor*/
bool gPrintActivity;

int gK; // radix
int gN; // dimension
int gC; // concentration

int gNodes;

// generate nocviewer trace
bool gTrace;

ostream *gWatchOut;

// Neurosim
int _k;
int _n;
int _header_size;
map<int, int> node_per_layer;
map<string, int> neurosim_map1;
map<string, int> neurosim_map2;
map<string, string> node_op;
map<string, int> node_loaction;
map<int, string> node_type;
map<string, int> activation_size;
map<int, float> injection_rate;
map<int, int> col;
map<int, vector<string>> input_node;

map<string, vector<int>> neurosim_map_small;
map<string, float> activation_size_small_send;
map<string, float> injection_rate_small_send;

map<string, float> activation_size_small_receive;
map<string, float> injection_rate_small_receive;
/////////////////////////////////////////////////////////////////////////////
vector<string> split(string input, char delimiter) {
  vector<string> result;
  stringstream ss(input);
  string tmp;

  while (getline(ss, tmp, delimiter))
    result.push_back(tmp);

  return result;
}
void print_vector(std::vector<int> const &input) {
  for (int i = 0; i < input.size(); i++) {
    std::cout << input.at(i) << ' ';
  }
}
/////////////////////////////////////////////////////////////////////////////

bool Simulate(BookSimConfig const &config, vector<string> input_node_name,
              vector<int> input_node_location, vector<int> cur_node_location,
              int input_activation_size, float inject) {
  vector<Network *> net;

  int subnets = config.GetInt("subnets");
  /*To include a new network, must register the network here
   *add an else if statement with the name of the network
   */
  net.resize(subnets);
  for (int i = 0; i < subnets; ++i) {
    ostringstream name;
    name << "network_" << i;
    net[i] = Network::New(config, name.str());
  }

  /*tcc and characterize are legacy
   *not sure how to use them
   */

  assert(trafficManager == NULL);
  trafficManager =
      TrafficManager::New(config, net, input_node_name, input_node_location,
                          cur_node_location, input_activation_size, inject);

  /*Start the simulation run
   */

  double total_time;                   /* Amount of time we've run */
  struct timeval start_time, end_time; /* Time before/after user code */
  total_time = 0.0;
  gettimeofday(&start_time, NULL);

  bool result = trafficManager->Run();

  gettimeofday(&end_time, NULL);
  total_time =
      ((double)(end_time.tv_sec) + (double)(end_time.tv_usec) / 1000000.0) -
      ((double)(start_time.tv_sec) + (double)(start_time.tv_usec) / 1000000.0);

  cout << "Total run time " << total_time << endl;

  for (int i = 0; i < subnets; ++i) {

    /// Power analysis
    if (config.GetInt("sim_power") > 0) {
      Power_Module pnet(net[i], config);
      pnet.run();
    }

    delete net[i];
  }

  delete trafficManager;
  trafficManager = NULL;

  return result;
}

// Example:
// /root/neurosim/booksim2/src/booksim \
//   /root/neurosim/booksim2/src/examples/mesh88_lat \
//   14 \ # config.k
//   1 \ # config_small1.k
//   1 \ # config_small2.k
//   12 \ # config.latency_per_flit
//   12 \ # config_small1.latency_per_flit
//   12 \ # config_small2.latency_per_flit
//   /root/neurosim/Inference_pytorch/CLUSTER_ResNet50_SA:128_PE:2_TL:32.txt \ #
//   mapping_file_path
//   /root/neurosim/Inference_pytorch/SMALL_ResNet50_SA:128_PE:2_TL:32.txt \ #
//   small_mapping_file_path 0.00328099 \ # config.wire_length_tile 0.00328099 \
//   # config_small1.wire_length_tile 0.00328099 \ #
//   config_small2.wire_length_tile 128 \ # config.flit_size 128 \ #
//   config_small1.flit_size 128 \ # config_small2.flit_size 0 \ # is_anynet n/a
//   \ # network_file (used if is_anynet is true) 3 \ # node 1 # type: BIG(1),
//   SMALL_send(2), SMALL_receive(3)
int main(int argc, char **argv) {

  BookSimConfig config;

  if (!ParseArgs(&config, argc, argv)) {
    cerr << "Usage: " << argv[0] << " configfile... [param=value...]" << endl;
    return 0;
  }

  /*initialize routing, traffic, injection functions
   */

  gPrintActivity = (config.GetInt("print_activity") > 0);
  gTrace = (config.GetInt("viewer_trace") > 0);
  string watch_out_file = config.GetStr("watch_out");
  if (watch_out_file == "") {
    gWatchOut = NULL;
  } else if (watch_out_file == "-") {
    gWatchOut = &cout;
  } else {
    gWatchOut = new ofstream(watch_out_file.c_str());
  }


  int is_anynet = stoi(argv[16]);
  config.Assign("k", stoi(argv[2]));
  if (config.GetStr("topology") == "fattree") {
    config.Assign("k", 2);
    int k_ = stoi(argv[2]);
    k_ = log_two(k_ * k_);
    config.Assign("n", k_);
  }
  config.Assign("latency_per_flit", stoi(argv[5]));
  config.Assign("wire_length_tile", stof(argv[10]) * 1000);
  // config.Assign("flit_size",stoi(argv[13]));
  config.Assign("flit_size", stoi(argv[13]));
  if (is_anynet) {
    config.Assign("network_file", argv[17]);
  }

  BookSimConfig config_small1;
  config_small1 = config;
  config_small1.Assign("k", stoi(argv[3]));
  if (config_small1.GetStr("topology") == "fattree") {
    config_small1.Assign("k", 2);
    int k_ = stoi(argv[3]);
    k_ = log_two(k_ * k_);
    config_small1.Assign("n", k_);
  }
  config_small1.Assign("latency_per_flit", stoi(argv[6]));
  config_small1.Assign("wire_length_tile", stof(argv[11]) * 1000);
  config_small1.Assign("flit_size", stoi(argv[14]));

  BookSimConfig config_small2;
  config_small2 = config;
  config_small2.Assign("k", stoi(argv[4]));
  if (config_small2.GetStr("topology") == "fattree") {
    config_small2.Assign("k", 2);
    int k_ = stoi(argv[4]);
    k_ = log_two(k_ * k_);
    config_small2.Assign("n", k_);
  }
  config_small2.Assign("latency_per_flit", stoi(argv[7]));
  config_small2.Assign("wire_length_tile", stof(argv[12]) * 1000);
  config_small2.Assign("flit_size", stoi(argv[15]));

  _k = config.GetInt("k");
  _n = config.GetInt("n");
  _header_size = config.GetInt("header_size");
  // _flit_size=config.GetInt("flit_size");

  // Neurosim
  string mapping_file_path = argv[8];

  if (mapping_file_path.empty() && config.GetStr("traffic") == "neurosim") {
    assert("Neurosim mode but there is no Neurosim mapping file");
  } else if (config.GetStr("traffic") == "neurosim") {
    // read File
    ifstream openFile;
    openFile.open(mapping_file_path);
    if (openFile.is_open()) {
      string line;
      while (getline(openFile, line)) {
        line.insert(line.length(), ",");
        std::string delimiter = ",";
        vector<std::string> tok{};
        size_t pos = 0;
        while ((pos = line.find(delimiter)) != string::npos) {
          tok.push_back(line.substr(0, pos));
          line.erase(0, pos + delimiter.length());
        }
        if (tok[0] != "node") {
          if (node_per_layer.find(stoi(tok[0])) == node_per_layer.end()) {
            node_per_layer[stoi(tok[0])] = 1;
          } else {
            node_per_layer[stoi(tok[0])] += 1;
          }
          int tok0 = stoi(tok[0]);
          tok[0] = tok[0] + '_' + to_string(node_per_layer[stoi(tok[0])]);
          if (tok[1] != "") {
            if (input_node.find(stoi(tok[1])) == input_node.end()) {
              input_node[stoi(tok[1])] = {tok[0]};
            } else {
              input_node[stoi(tok[1])].push_back(tok[0]);
            }
            neurosim_map1[tok[0]] = stoi(tok[1]);
          }
          if (tok[2] != "") {
            if (input_node.find(stoi(tok[2])) == input_node.end()) {
              input_node[stoi(tok[2])] = {tok[0]};
            } else {
              input_node[stoi(tok[2])].push_back(tok[0]);
            }
            neurosim_map2[tok[0]] = stoi(tok[2]);
          }

          node_op[tok[0]] = tok[3];
          vector<std::string> tok_loc = split(tok[4], '-');
          node_loaction[tok[0]] = stoi(tok_loc[0]) * _k + stoi(tok_loc[1]);


          if (is_anynet) {
            node_loaction[tok[0]] = stoi(tok[10]);
          }
          if (config.GetStr("topology") == "fattree") {
            if (stoi(tok_loc[1]) == 0) {
              std::cout << tok_loc << endl;
              node_loaction[tok[0]] = powi(2, config.GetInt("n"));
            }
          }
          
          node_type[stoi(tok[0])] = tok[5];
          activation_size[tok[0]] = stoi(tok[6]);
          // activation_size[tok[0]] = ceil(pkt_size / _flit_size);
          injection_rate[tok0] = stof(tok[7]);
          col[tok0] = stoi(tok[9]);
        }
      }
      openFile.close();
    }
    string small_mapping_file_path = argv[9];
    ifstream openFile_small;
    openFile_small.open(small_mapping_file_path.data());
    if (openFile_small.is_open()) {
      string line;
      while (getline(openFile_small, line)) {
        line.insert(line.length(), ",");
        std::string delimiter = ",";
        vector<std::string> tok_small{};
        size_t pos = 0;
        while ((pos = line.find(delimiter)) != string::npos) {
          tok_small.push_back(line.substr(0, pos));
          line.erase(0, pos + delimiter.length());
        }

        // cout<<tok_small<<endl;
        if (tok_small[0] != "node") {
          // neurosimmap
          std::string newstr1 =
              tok_small[1].substr(0, tok_small[1].length() - 1);
          istringstream iss(newstr1);
          char separator = ' ';
          std::string str_buf;
          while (getline(iss, str_buf, separator)) {
            if (str_buf != "\0") {
              if (std::stoi(str_buf) >= 0) {
                neurosim_map_small[tok_small[0]].push_back(std::stoi(str_buf));
              }
            }
          }
          activation_size_small_send[tok_small[0]] = stoi(tok_small[2]);
          injection_rate_small_send[tok_small[0]] = stof(tok_small[3]);
          activation_size_small_receive[tok_small[0]] = stoi(tok_small[4]);
          injection_rate_small_receive[tok_small[0]] = stof(tok_small[5]);
        }
      }
    }
  }

  /*configure and run the simulator
   */
  map<int, int>::iterator iter;
  for (iter = node_per_layer.begin(); iter != node_per_layer.end(); iter++) {
    int node = iter->first;
    if (node == stoi(argv[argc - 2])) {
      cout << "node" << node << endl;
      // cout << "node" <<node <<endl;
      cout << "node" << node << endl;
      cout << "argv" << argv[argc - 2] << endl;
      vector<int> input_location = {};
      int input_activation;
      float inject;
      for (string i : input_node.find(node)->second) {
        input_location.push_back(node_loaction.find(i)->second);
      }
      vector<int> cur_node_location = {};
      for (int i = 1; i < iter->second + 1; i++) {
        cur_node_location.push_back(
            node_loaction.find(to_string(node) + "_" + to_string(i))->second);
        input_activation =
            ceil((activation_size.find(to_string(node) + "_" + to_string(i))
                      ->second +
                  _header_size) /
                 config.GetInt("flit_size"));
        cout << "activation_size: "
             << activation_size.find(to_string(node) + "_" + to_string(i))
                    ->second
             << endl;
      }

      inject = injection_rate.find(node)->second;
      cout << node_type.find(node)->second << endl;
      // cout<<"Time taken node " << node<<endl;
      // cout<<"Time taken node " << iter->second;
      // cout << input_location << endl;
      // cout << input_activation << endl;
      // cout << cur_node_location << endl;
      
      // cout<<"jjj"<<endl;
      cout << inject << endl;
      cout << "type" << argv[argc - 1] << endl;


      if (stoi(argv[argc - 1]) == 1) { // BIG
        config.Assign("injection_rate", inject);
        InitializeRoutingMap(config);
        Simulate(config, input_node.find(node)->second, input_location,
                 cur_node_location, input_activation, inject);
      } else if (stoi(argv[argc - 1]) == 2) { //"SMALL_send"
        cout << node_type.find(node)->second << endl;
        if (node_type.find(node)->second == "chip1" &&
            stoi(argv[3]) != 1) {
          vector<string> input_node_small = {"0"};
          vector<int> input_location_small = {0};
          if (config_small1.GetStr("topology") == "fattree") {
            vector<int> input_location_small ={powi(2, config_small1.GetInt("n"))};
          }
          vector<int> cur_node_location_small =
              neurosim_map_small.find(to_string(node) + "_0")->second;
          cur_node_location_small.erase(cur_node_location_small.begin());
          int input_activation_small = ceil(
              activation_size_small_send.find(to_string(node) + "_0")->second /
              config_small1.GetInt("flit_size"));
          float inject_small =
              injection_rate_small_send.find(to_string(node) + "_0")->second;
          print_vector(cur_node_location_small);
          config_small1.Assign("injection_rate", inject_small);
          InitializeRoutingMap(config_small1);
          Simulate(config_small1, input_node_small, input_location_small,
                   cur_node_location_small, input_activation_small,
                   inject_small);
        } else if (node_type.find(node)->second == "chip2" &&
                   stoi(argv[4]) != 1) {
          cout << "llll" << endl;
          vector<string> input_node_small = {"0"};
          vector<int> input_location_small = {0};
          if (config_small2.GetStr("topology") == "fattree") {
            vector<int> input_location_small ={powi(2, config_small2.GetInt("n"))};
          }
          cout << to_string(node)
               << neurosim_map_small.find(to_string(node) + "_0")->second
               << endl;
          vector<int> cur_node_location_small =
              neurosim_map_small.find(to_string(node) + "_0")->second;
          cur_node_location_small.erase(cur_node_location_small.begin());
          cout << "llll" << endl;
          int input_activation_small = ceil(
              activation_size_small_send.find(to_string(node) + "_0")->second /
              config_small2.GetInt("flit_size"));
          float inject_small =
              injection_rate_small_send.find(to_string(node) + "_0")->second;
          print_vector(cur_node_location_small);
          config_small2.Assign("injection_rate", inject_small);
          InitializeRoutingMap(config_small2);
          Simulate(config_small2, input_node_small, input_location_small,
                   cur_node_location_small, input_activation_small,
                   inject_small);
        } else {
          cout << "Time taken is " << 0 << " cycles" << endl;
          cout << "- Total Power: " << 0 << endl;
          cout << "- Total Area: " << 0 << endl;
          cout << "- Total leak Power: " << 0 << endl;
        }
      } else if (stoi(argv[argc - 1]) == 3) { //""SMALL_receive""
        if (node_type.find(node)->second == "chip1" &&
            stoi(argv[3])!= 1) {
          vector<string> input_node_small = {};
          vector<int>::iterator ptr;
          for (ptr = neurosim_map_small.find(to_string(node) + "_0")
                         ->second.begin();
               ptr !=
               neurosim_map_small.find(to_string(node) + "_0")->second.end();
               ++ptr) {
            input_node_small.push_back(to_string(*ptr));
          }
          input_node_small.erase(input_node_small.begin());

          vector<int> input_location_small =
              neurosim_map_small.find(to_string(node) + "_0")->second;
          input_location_small.erase(input_location_small.begin());
          vector<int> cur_node_location_small = {0};
          
          if (config_small1.GetStr("topology") == "fattree") {
            vector<int> cur_node_location_small ={powi(2, config_small1.GetInt("n"))};
          }
          

          int input_activation_small =
              ceil(activation_size_small_receive.find(to_string(node) + "_0")
                       ->second /
                   config_small1.GetInt("flit_size"));
          float inject_small =
              injection_rate_small_receive.find(to_string(node) + "_0")->second;
          cout << input_activation_small << endl;
          ;
          cout << inject_small << endl;

          print_vector(input_location_small);
          print_vector(cur_node_location_small);
          config_small1.Assign("injection_rate", inject_small);
          InitializeRoutingMap(config_small1);
          Simulate(config_small1, input_node_small, input_location_small,
                   cur_node_location_small, input_activation_small,
                   inject_small);
        } else if (node_type.find(node)->second == "chip2" &&
                   stoi(argv[4]) != 1) {
          vector<string> input_node_small = {};
          vector<int>::iterator ptr;
          for (ptr = neurosim_map_small.find(to_string(node) + "_0")
                         ->second.begin();
               ptr !=
               neurosim_map_small.find(to_string(node) + "_0")->second.end();
               ++ptr) {
            input_node_small.push_back(to_string(*ptr));
          }
          input_node_small.erase(input_node_small.begin());
          vector<int> input_location_small =
              neurosim_map_small.find(to_string(node) + "_0")->second;
          input_location_small.erase(input_location_small.begin());
          vector<int> cur_node_location_small = {0};
          if (config_small2.GetStr("topology") == "fattree") {
            vector<int> cur_node_location_small ={powi(2, config_small2.GetInt("n"))};
          }

          int input_activation_small =
              ceil(activation_size_small_receive.find(to_string(node) + "_0")
                       ->second /
                   config_small2.GetInt("flit_size"));
          float inject_small =
              injection_rate_small_receive.find(to_string(node) + "_0")->second;
          cout << input_activation_small << endl;
          ;
          cout << inject_small << endl;
          print_vector(cur_node_location_small);
          config_small2.Assign("injection_rate", inject_small);
          InitializeRoutingMap(config_small2);
          Simulate(config_small2, input_node_small, input_location_small,
                   cur_node_location_small, input_activation_small,
                   inject_small);
        } else {
          cout << "Time taken is " << 0 << " cycles" << endl;
          cout << "- Total Power: " << 0 << endl;
          cout << "- Total Area: " << 0 << endl;
          cout << "- Total leak Power: " << 0 << endl;
        }
      }

      // cout << "result" <<result <<endl;
      // return result ? -1 : 0;

      return 0;
    }
  }
}
//
