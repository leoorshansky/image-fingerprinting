#include <iostream>
#include <memory>
#include <random>
#include <unordered_map>
#include <map>
#include <queue>
#include <experimental/filesystem>
#include "src/pHash.h"
#include <fstream>
#include <boost/serialization/unordered_map.hpp>
#include <boost/serialization/utility.hpp>
#include <boost/archive/text_iarchive.hpp>
#include <boost/archive/text_oarchive.hpp>
#include <boost/program_options.hpp>

using namespace std;
using namespace cimg_library;
namespace fs = std::experimental::filesystem::v1;
namespace po = boost::program_options;

// Side length in pixels of the square fingerprint regions
int REGION_SIZE = 50;
// Number of slices to split each side for the proportional image matching
int SLICE_NUMBER = 5;
// Number of samples to take from the image for matching
int SAMPLES = 5000;

int computeFingerprint(const CImg<uint8_t> &image)
{
  vector<int> channelMeans;
  unsigned long long numPixels = image.width() * image.height();
  for (int channel = 0; channel < 3; channel++)
  {
    unsigned long long sum;
    for (int x = 0; x < image.width(); x++)
    {
      for (int y = 0; y < image.height(); y++)
      {
        sum += image(x, y, 0, channel);
      }
    }
    channelMeans.push_back(sum / numPixels);
  }

  return (channelMeans[0] << 16) | (channelMeans[1] << 8) | channelMeans[0];
}

class fingerprint_t {
  friend class boost::serialization::access;
  public:
    unordered_multimap<int, pair<string, int>> data;
    unordered_map<ulong64, string> pHashes;
    template<class Archive>
    void serialize(Archive & ar, const unsigned int version) {
      ar & data;
      ar & pHashes;
    }
};

int main(int argc, char **argv)
{
  // Command line inputs
  string imageArg;
  string dirArg;
  string saveArg;
  bool verbose;

  // Define command line arguments
  po::options_description desc("Perform an image fingerprint search in the specified directory. Usage: ./fingerprint-search [image] [search-dir]");
  desc.add_options()("help", "Returns help message.")("image", po::value<string>(&imageArg)->required(), "Image file to search for.")("search-dir", po::value<string>(&dirArg)->required(), "Directory to search in.")("output,O", po::value<string>(&saveArg)->default_value("hashes"), "Output file for the constructed image index.")("load-index,L", po::value<string>(), "Input file for pre-constructed image index.")("region-size,RS", po::value<int>(&REGION_SIZE), "Side length of square fingerprinting regions, in pixels.")("samples,S", po::value<int>(&SAMPLES), "Number of samples to take from the input image for matching.")("verbose,V", po::bool_switch(&verbose), "Print debug output.");

  // Add positional arguments to substitute for some of the keyword args
  po::positional_options_description pdesc;
  pdesc.add("image", 1);
  pdesc.add("search-dir", 1);

  po::variables_map vm;
  po::store(po::command_line_parser(argc, argv).options(desc).positional(pdesc).run(), vm);

  // Display help page if necessary before calling notify() and then exit
  // This is because notify() would exit and whine about missing required arguments
  if (vm.count("help"))
  {
    cout << desc << endl;
    return 1;
  }

  po::notify(vm);

  fs::path dirPath{dirArg};
  // Map to store the image fingerprints
  fingerprint_t fp_data;

  unordered_multimap<int, pair<string, int>> fingerprints;
  unordered_map<ulong64, string> pHashes;

  // If present, load the index from a pre-computed file and skip the indexing step
  if (vm.count("load-index"))
  {
    if (verbose)
      cout << "DEBUG: Loading index ..." << flush;
    ifstream ifs(vm["load-index"].as<string>());
    boost::archive::text_iarchive ia(ifs);
    ia >> fp_data;
    ifs.close();
    fingerprints = fp_data.data;
    pHashes = fp_data.pHashes;
    if (verbose)
      cout << " Done." << endl;
  }
  else
  {
    // Go through each image in the search directory and compute its perceptual hash (fingerprint)
    for (const auto &entry : fs::directory_iterator(dirPath))
    {
      // The for loop discovers each child of the directory, image file or not
      // Check that it's a file and not a directory
      if (fs::is_regular_file(entry))
      {
        // Try to load the image, skip it if it doesn't work
        // image.load() invokes the magick executable which must be included in the folder
        CImg<uint8_t> image;
        auto imageName = entry.path().string();
        if (verbose)
          cout << "DEBUG: Indexing " + imageName + " ..." << flush;
        try
        {
          image.load(imageName.c_str());
        }
        catch (CImgIOException ex)
        {
          continue;
        }

        // First line of detection, whole image pHash
        ulong64 pHash;
        ph_dct_imagehash(imageName.c_str(), pHash);
        pHashes[pHash] = imageName;

        // For every disjoint (excluding edges) REGION_SIZE x REGION_SIZE square in the image,
        // compute its fingerprint. Add it to the index.
        CImg<uint8_t> square;
        int fingerprint;
        for (int x = REGION_SIZE; x < image.width(); x += REGION_SIZE)
        {
          for (int y = REGION_SIZE; y < image.height(); y += REGION_SIZE)
          {
            square = image.get_crop(x - REGION_SIZE, y - REGION_SIZE, 0, 0, x, y, 0, 2);
            fingerprint = computeFingerprint(square);
            fingerprints.insert({fingerprint, {imageName, x}});
          }
          // Compute the hash from the bottom edge
          square = image.get_crop(x - REGION_SIZE, image.height() - REGION_SIZE, 0, 0, x, image.height(), 0, 2);
          fingerprint = computeFingerprint(square);
          fingerprints.insert({fingerprint, {imageName, x}});
        }
        // Repeat along the right edge
        for (int y = REGION_SIZE; y < image.height(); y += REGION_SIZE)
        {
          square = image.get_crop(image.width() - REGION_SIZE, y - REGION_SIZE, 0, 0, image.width(), y, 0, 2);
          fingerprint = computeFingerprint(square);
          fingerprints.insert({fingerprint, {imageName, image.width()}});
        }
        // Repeat for the bottom right corner
        square = image.get_crop(image.width() - REGION_SIZE, image.height() - REGION_SIZE, 0, 0, image.width(), image.height(), 0, 2);
        fingerprint = computeFingerprint(square);
        fingerprints.insert({fingerprint, {imageName, image.width()}});

        if (verbose)
          cout << " Done." << endl;
      }
    }

    if (verbose)
      cout << "DEBUG: Saving index ..." << flush;
    fingerprint_t fp_data{fingerprints, pHashes};
    ofstream ofs(saveArg);
    boost::archive::text_oarchive oa(ofs);
    oa << fp_data;
    ofs.close();
    if (verbose)
      cout << " Done." << endl;
  }

  // Repeat fingerprinting procedure for the supplied image

  if (verbose)
    cout << "DEBUG: Fingerprinting image ..." << flush;
  CImg<uint8_t> image;
  try
  {
    image.load(imageArg.c_str());
  }
  catch (CImgIOException e)
  {
    cout << "Could not open image file." << endl;
    return -1;
  }

  // pHash detection procedure
  ulong64 hash;
  ph_dct_imagehash(imageArg.c_str(), hash);
  if (pHashes.count(hash)) {
    cout << " Done.\n";
    cout << "Exact Match Found: " << pHashes[hash] << endl;
    return 0;
  }

  // Sampled fingerprints from the image
  vector<pair<int, int>> imageHashes;

  CImg<uint8_t> square;
  default_random_engine random(21);
  // For every disjoint (exlcluding edges) REGION_SIZE x REGION_SIZE square in the image,
  // compute its fingerprint using phash. Add it to the index.
  for (int i = 0; i < SAMPLES; i++)
  {
    int x = (random() % (image.width() - REGION_SIZE)) + REGION_SIZE;
    int y = (random() % (image.height() - REGION_SIZE)) + REGION_SIZE;
    square = image.get_crop(x - REGION_SIZE, y - REGION_SIZE, 0, 0, x, y, 0, 2);
    auto fingerprint = computeFingerprint(square);
    imageHashes.push_back({fingerprint, x});
  }

  if (verbose)
    cout << " Done." << endl;

  // Perform fast fingerprint lookup and identify images with hits
  if (verbose)
    cout << "DEBUG: Finding matches ..." << flush;
  unordered_map<string, map<float, int>> hits;
  for (const auto &p : imageHashes)
  {
    auto hash = p.first;
    auto x = p.second;
    if (fingerprints.count(hash))
    {
      auto itr = fingerprints.equal_range(hash);
      for (auto it = itr.first; it != itr.second; it++)
      {
        int dist = it->second.second - x;
        hits[it->second.first][dist]++;
      }
    }
  }
  if (verbose)
    cout << " Done." << endl;

  if (hits.empty())
  {
    cout << "No matches found." << endl;
    return 0;
  }

  int max = 0;
  string bestMatch;
  for (const auto &kvpair : hits)
  {
    if (verbose)
      cout << kvpair.first + ": ";
    int imgMax = 0;
    queue<int> lastTen;
    int runningSum = 0;
    for (const auto &kvpair2 : kvpair.second)
    {
      runningSum += kvpair2.second;
      lastTen.push(kvpair2.second);
      if (lastTen.size() < 10)
        continue;
      if (runningSum > imgMax)
        imgMax = runningSum;
      runningSum -= lastTen.front();
      lastTen.pop();
    }
    if (imgMax > max)
    {
      max = imgMax;
      bestMatch = kvpair.first;
    }
    if (verbose)
      cout << imgMax << endl;
  }

  cout << "Best Match: " + bestMatch + " with " << max << " matches." << endl;

  return 0;
}