

/*==========================================================================
 * Copyright (c) 2018 RMIT University.  All Rights Reserved.
 *
 * Use of the Lemur Toolkit for Language Modeling and Information Retrieval
 * is subject to the terms of the software license set forth in the LICENSE
 * file included with this software, and also available at
 * http://www.lemurproject.org/license.html
 *
 *==========================================================================
*/

//
// BM25F
//
// 08 October 2018
//

#include "indri/Parameters.hpp"
#include "QueryBM25F.hpp"

static void parse_field_spec(std::vector<std::string> &keys,
                             std::vector<double> &values,
                             const std::string& spec) {
  int nextComma = 0;
  int nextColon = 0;
  int  location = 0;

  for( location = 0; location < spec.length(); ) {
    nextComma = spec.find( ',', location );
    nextColon = spec.find( ':', location );

    std::string key = spec.substr( location, nextColon-location );
    double value = std::stod(spec.substr( nextColon+1, nextComma-nextColon-1 ));

    keys.push_back(key);
    values.push_back(value);

    if( nextComma > 0 )
      location = nextComma+1;
    else
      location = spec.size();
  }

  return;
}

static void usage(indri::api::Parameters param) {
  if (!param.exists("index") || !param.exists("query")
      || !param.exists("fieldB") || !param.exists("fieldWt") || !param.exists("k1")) {
    std::cerr << "bm25f usage: " << std::endl
              << "  bm25f -index=myindex -qno=1 -query=myquery -count=1000 -k1=10 -fieldB=title:8,body:2 -fieldWt=title:6,body:1" << std::endl
              << std::endl;
    exit(-1);
  }
}

int main( int argc, char** argv ) {
  try {
    cerr << "Built with " << INDRI_DISTRIBUTION << endl;

    indri::api::Parameters& param = indri::api::Parameters::instance();
    param.loadCommandLine( argc, argv );
    usage( param );

    std::string index = param["index"];
    std::string stemmer = param.get("stemmer", "krovetz");
    std::string query = param["query"];
    std::string qno = param.get("qno", "1");
    int requested = param.get("count", 1000);

    double k1 = param.get("k1");
    std::vector<std::string> fields;
    std::vector<std::string> fields2;
    std::vector<double> fieldB;
    std::vector<double> fieldWt;
    parse_field_spec(fields, fieldB, param["fieldB"]);
    parse_field_spec(fields2, fieldWt, param["fieldWt"]);
    if (fields.size() != fields2.size()) {
      std::cerr << "Please specify same fields in \"fieldB\" and \"fieldWt\"" << std::endl;
      return -1;
    }
    for (size_t fieldIndex = 0; fieldIndex < fields.size(); ++fieldIndex) {
      if (fields[fieldIndex] != fields2[fieldIndex]) {
        std::cerr << "Please specify same fields in \"fieldB\" and \"fieldWt\"" << std::endl;
        std::cerr << fields[fieldIndex] << " " << fields2[fieldIndex] << std::endl;
        return -1;
      }
    }


    QueryBM25F bm25f(index, stemmer, fields, fieldB, fieldWt, k1, requested);
    std::vector<std::pair<std::string, double>> result = bm25f.query(query);

    // 1 Q0 clueweb09-en0007-63-02101 1 -3.34724 indri
    int rank = 1;
    for (size_t i = 0; i < result.size(); ++i) {
      std::string docno = result[i].first;
      double score = result[i].second;
      std::cout << qno << " Q0 " << docno << " " << i + 1
                << " " << score << " " << "bm25f" << std::endl;
      rank += 1;
    }

  }
  catch( lemur::api::Exception& e ) {
    LEMUR_ABORT(e);
  } catch( ... ) {
    std::cout << "Caught unhandled exception" << std::endl;
    return -1;
  }

  return 0;
}

