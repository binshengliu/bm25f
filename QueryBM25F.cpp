#include "indri/Parameters.hpp"
#include "indri/RelevanceModel.hpp"
#include "indri/LocalQueryServer.hpp"
#include "indri/ScopedLock.hpp"
#include "indri/DocExtentListIterator.hpp"
#include "QueryBM25F.hpp"
#include <queue>
#include <cmath>

bool DocScore::greater::operator () (const DocScore &lhs, const DocScore &rhs) const {
      return lhs.score > rhs.score;
}


bool DocIterator::greater::operator () (indri::index::DocListIterator *lhs, indri::index::DocListIterator *rhs) const {
      return lhs->currentEntry()->document > rhs->currentEntry()->document;
}

bool DocIterator::field_greater::operator () (indri::index::DocExtentListIterator *lhs,
                      indri::index::DocExtentListIterator *rhs) const {
      return lhs->currentEntry()->document > rhs->currentEntry()->document;
}

DocIterator::DocIterator(indri::index::Index *index,
                         const std::vector<std::string> &fields,
                         const std::vector<std::string> &stems):
    _currentIter(NULL),
    _termIters(stems.size(), NULL),
    _fieldIters(fields.size(), NULL),
    _termFieldOccur(stems.size(), vector<int>(fields.size(), 0)),
    _fieldLength(fields.size(), 0) {
  for (size_t termIndex = 0; termIndex < stems.size(); ++termIndex) {
    auto *iter = index->docListIterator(stems[termIndex]);
    if (iter) {
      iter->startIteration();
      if (!iter->finished()) {
        _termItersQueue.push(iter);
        _termIters[termIndex] = iter;
      }
    }
  }

  for (size_t fieldIndex = 0; fieldIndex < fields.size(); ++fieldIndex) {
    auto *iter = index->fieldListIterator(fields[fieldIndex]);
    if (iter) {
      iter->startIteration();
      if (!iter->finished()) {
        _fieldIters[fieldIndex] = iter;
      }
    }
  }

  forwardFieldIter();

  if (!isAtValidEntry()) {
    nextEntry();
  }
}

bool DocIterator::isAtValidEntry() {
  if (_termItersQueue.empty()) {
    return false;
  }

  lemur::api::DOCID_T document = _termItersQueue.top()->currentEntry()->document;
  for (auto tIter: _termIters) {
    if (!tIter || tIter->finished() || tIter->currentEntry()->document != document) {
      continue;
    }

    for (auto fIter: _fieldIters) {
      if (!fIter || fIter->finished() || fIter->currentEntry()->document != document) {
        continue;
      }

      for (auto &e: fIter->currentEntry()->extents) {
        for (auto pos: tIter->currentEntry()->positions) {
          if (pos >= e.begin && pos < e.end) {
            return true;
          }
        }
      }
    }
  }

  return false;
}

DocIterator::entry DocIterator::currentEntry() {
  lemur::api::DOCID_T document = _termItersQueue.top()->currentEntry()->document;
  updateStats();

  DocIterator::entry e;
  e.document = document;
  e.termFieldOccurrences = &_termFieldOccur;
  e.fieldLength = &_fieldLength;
  return e;
}

void DocIterator::nextEntry() {
  nextDocEntry();
  while (!isAtValidEntry() && !finished()) {
    nextDocEntry();
  }

  return;
}

void DocIterator::resetStats() {
  for (auto &field: _termFieldOccur) {
    std::fill(field.begin(), field.end(), 0);
  }

  std::fill(_fieldLength.begin(), _fieldLength.end(), 0);
}

void DocIterator::updateStats() {
  resetStats();
  if (_termItersQueue.empty()) {
    return;
  }

  lemur::api::DOCID_T document = _termItersQueue.top()->currentEntry()->document;
  for (size_t termIndex = 0; termIndex < _termIters.size(); ++termIndex) {
    auto *tIter = _termIters[termIndex];
    if (!tIter || tIter->finished() || tIter->currentEntry()->document != document) {
      continue;
    }

    for (size_t fieldIndex = 0; fieldIndex < _fieldIters.size(); ++fieldIndex) {
      auto fIter = _fieldIters[fieldIndex];
      if (!fIter || fIter->finished() || fIter->currentEntry()->document != document) {
        continue;
      }

      for (auto &e: fIter->currentEntry()->extents) {
        for (auto pos: tIter->currentEntry()->positions) {
          if (pos >= e.begin && pos < e.end) {
            _termFieldOccur[termIndex][fieldIndex] += 1;
          }
        }
      }
    }
  }

  for (size_t fieldIndex = 0; fieldIndex < _fieldIters.size(); ++fieldIndex) {
    auto fIter = _fieldIters[fieldIndex];
    if (!fIter || fIter->finished() || fIter->currentEntry()->document != document) {
      continue;
    }

    for (auto &e: fIter->currentEntry()->extents) {
      _fieldLength[fieldIndex] += e.end - e.begin;
    }
  }

  return;
}

void DocIterator::forwardFieldIter() {
  for (auto &iter: _fieldIters) {
    if (iter) {
      iter->nextEntry(_termItersQueue.top()->currentEntry()->document);
    }
  }
}

void DocIterator::nextDocEntry() {
  if (_termItersQueue.empty()) {
    return;
  }

  indri::index::DocListIterator *iter = _termItersQueue.top();
  lemur::api::DOCID_T lastId = iter->currentEntry()->document;
  while (!_termItersQueue.empty() && _termItersQueue.top()->currentEntry()->document <= lastId) {
    indri::index::DocListIterator *iter = _termItersQueue.top();
    _termItersQueue.pop();
    if (iter->nextEntry(lastId + 1)) {
      _termItersQueue.push(iter);
    }
  }

  if (_termItersQueue.empty()) {
    return;
  }

  forwardFieldIter();
  return;
}

bool DocIterator::finished() {
  return _termItersQueue.empty();
}

QueryBM25F::QueryBM25F(std::string index,
                       std::string stemmer,
                       std::vector<std::string> fields,
                       std::vector<double> fieldB,
                       std::vector<double> fieldWt,
                       double k1,
                       int requested):
    _fields(fields),
    _fieldB(fieldB),
    _fieldWt(fieldWt),
    _k1(k1),
    _requested(requested),
    _avgFieldLen(fields.size(), 0)
{
  indri::api::Parameters p;
  p.set("stemmer.name", stemmer);
  _repo.openRead(index, &p);

  indri::collection::Repository::index_state state = _repo.indexes();
  _index = (*state)[0];
  _totalDocumentCount = _index->documentCount();

  // Average field length;
  for (size_t fieldIndex = 0; fieldIndex < fields.size(); ++fieldIndex) {
    double len = _index->fieldTermCount(fields[fieldIndex]) / _totalDocumentCount;
    _avgFieldLen[fieldIndex] = len;
  }

  _environment.addIndex(index);
};

std::vector<std::pair<std::string, double>> QueryBM25F::query(std::string query) {
  std::vector<std::string> stems;
  std::istringstream buf(query);
  std::istream_iterator<std::string> beg(buf), end;
  for (auto t: std::vector<std::string>(beg, end)) {
    stems.push_back(_repo.processTerm(t));
  }

  std::vector<double> termDocCounts(stems.size(), 0);
  std::vector<double> termIdf(stems.size(), 0);
  for (size_t termIndex = 0; termIndex < stems.size(); ++termIndex) {
    double count = _index->documentCount(stems[termIndex]);
    termDocCounts[termIndex] = count;
    termIdf[termIndex] = log((_totalDocumentCount - count + 0.5) / (count + 0.5));
  }

  std::priority_queue<DocScore, vector<DocScore>, DocScore::greater> queue;
  double threshold = 0;
  DocIterator docIters(_index, _fields, stems);
  while (!docIters.finished()) {
    auto de = docIters.currentEntry();

    double score = 0;
    for (size_t termIndex = 0; termIndex < stems.size(); ++termIndex) {
      double pseudoFreq = 0;
      const std::vector<int> &fieldStats = de.termFieldOccurrences->at(termIndex);
      for (size_t fieldIndex = 0; fieldIndex < _fields.size(); ++fieldIndex) {
        int occurrences = fieldStats[fieldIndex];
        if (occurrences == 0 || de.fieldLength->at(fieldIndex) == 0) {
          continue;
        }

        double fieldFreq = occurrences / (1 + _fieldB[fieldIndex] * (de.fieldLength->at(fieldIndex) / _avgFieldLen[fieldIndex] - 1));
        pseudoFreq += _fieldWt[fieldIndex] * fieldFreq;
      }
      double tf = pseudoFreq / (_k1 + pseudoFreq);

      double idf = termIdf[termIndex];

      score += tf * idf;
    }

    if (queue.size() < _requested || score > threshold) {
      queue.push(DocScore(de.document, score));
      while (queue.size() > _requested) {
        queue.pop();
      }
      threshold = queue.top().score;
    }

    docIters.nextEntry();
  }

  std::vector<DocScore> s;
  std::vector<lemur::api::DOCID_T> docids;
  while (queue.size()) {
    DocScore result = queue.top();
    queue.pop();
    s.push_back(result);
    docids.push_back(result.id);
  }

  std::vector<std::string> docnos = _environment.documentMetadata(docids, "docno");

  std::vector<std::pair<std::string, double>> result;
  for (int i = s.size() - 1; i >= 0; --i) {
    result.push_back(std::make_pair(docnos[i], s[i].score));
  }

  return result;
}
