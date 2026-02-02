#include "ExplosionVolumeSystem.h"
#include <glad/glad.h>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <algorithm>
#include <cmath>
#include <random>

namespace {
// Standard Marching Cubes lookup tables
static const int edgeTable[256] = {
0x0,0x109,0x203,0x30a,0x406,0x50f,0x605,0x70c,0x80c,0x905,0xa0f,0xb06,0xc0a,0xd03,0xe09,0xf00,
0x190,0x99,0x393,0x29a,0x596,0x49f,0x795,0x69c,0x99c,0x895,0xb9f,0xa96,0xd9a,0xc93,0xf99,0xe90,
0x230,0x339,0x33,0x13a,0x636,0x73f,0x435,0x53c,0xa3c,0xb35,0x83f,0x936,0xe3a,0xf33,0xc39,0xd30,
0x3a0,0x2a9,0x1a3,0xaa,0x7a6,0x6af,0x5a5,0x4ac,0xbac,0xaa5,0x9af,0x8a6,0xfaa,0xea3,0xda9,0xca0,
0x460,0x569,0x663,0x76a,0x66,0x16f,0x265,0x36c,0xc6c,0xd65,0xe6f,0xf66,0x86a,0x963,0xa69,0xb60,
0x5f0,0x4f9,0x7f3,0x6fa,0x1f6,0xff,0x3f5,0x2fc,0xdfc,0xcf5,0xfff,0xef6,0x9fa,0x8f3,0xbf9,0xaf0,
0x650,0x759,0x453,0x55a,0x256,0x35f,0x55,0x15c,0xe5c,0xf55,0xc5f,0xd56,0xa5a,0xb53,0x859,0x950,
0x7c0,0x6c9,0x5c3,0x4ca,0x3c6,0x2cf,0x1c5,0xcc,0xfcc,0xec5,0xdcf,0xcc6,0xbca,0xac3,0x9c9,0x8c0,
0x8c0,0x9c9,0xac3,0xbca,0xcc6,0xdcf,0xec5,0xfcc,0xcc,0x1c5,0x2cf,0x3c6,0x4ca,0x5c3,0x6c9,0x7c0,
0x950,0x859,0xb53,0xa5a,0xd56,0xc5f,0xf55,0xe5c,0x15c,0x55,0x35f,0x256,0x55a,0x453,0x759,0x650,
0xaf0,0xbf9,0x8f3,0x9fa,0xef6,0xfff,0xcf5,0xdfc,0x2fc,0x3f5,0xff,0x1f6,0x6fa,0x7f3,0x4f9,0x5f0,
0xb60,0xa69,0x963,0x86a,0xf66,0xe6f,0xd65,0xc6c,0x36c,0x265,0x16f,0x66,0x76a,0x663,0x569,0x460,
0xca0,0xda9,0xea3,0xfaa,0x8a6,0x9af,0xaa5,0xbac,0x4ac,0x5a5,0x6af,0x7a6,0xaa,0x1a3,0x2a9,0x3a0,
0xd30,0xc39,0xf33,0xe3a,0x936,0x83f,0xb35,0xa3c,0x53c,0x435,0x73f,0x636,0x13a,0x33,0x339,0x230,
0xe90,0xf99,0xc93,0xd9a,0xa96,0xb9f,0x895,0x99c,0x69c,0x795,0x49f,0x596,0x29a,0x393,0x99,0x190,
0xf00,0xe09,0xd03,0xc0a,0xb06,0xa0f,0x905,0x80c,0x70c,0x605,0x50f,0x406,0x30a,0x203,0x109,0x0
};

static const int triTable[256][16] = {
{-1},
{0, 8, 3, -1},
{0, 1, 9, -1},
{1, 8, 3, 9, 8, 1, -1},
{1, 2, 10, -1},
{0, 8, 3, 1, 2, 10, -1},
{9, 2, 10, 0, 2, 9, -1},
{2, 8, 3, 2, 10, 8, 10, 9, 8, -1},
{3, 11, 2, -1},
{0, 11, 2, 8, 11, 0, -1},
{1, 9, 0, 2, 3, 11, -1},
{1, 11, 2, 1, 9, 11, 9, 8, 11, -1},
{3, 10, 1, 11, 10, 3, -1},
{0, 10, 1, 0, 8, 10, 8, 11, 10, -1},
{3, 9, 0, 3, 11, 9, 11, 10, 9, -1},
{9, 8, 10, 10, 8, 11, -1},
{4, 7, 8, -1},
{4, 3, 0, 7, 3, 4, -1},
{0, 1, 9, 8, 4, 7, -1},
{4, 1, 9, 4, 7, 1, 7, 3, 1, -1},
{1, 2, 10, 8, 4, 7, -1},
{3, 4, 7, 3, 0, 4, 1, 2, 10, -1},
{9, 2, 10, 9, 0, 2, 8, 4, 7, -1},
{2, 10, 9, 2, 9, 7, 2, 7, 3, 7, 9, 4, -1},
{8, 4, 7, 3, 11, 2, -1},
{11, 4, 7, 11, 2, 4, 2, 0, 4, -1},
{9, 0, 1, 8, 4, 7, 2, 3, 11, -1},
{4, 7, 11, 9, 4, 11, 9, 11, 2, 9, 2, 1, -1},
{3, 10, 1, 3, 11, 10, 7, 8, 4, -1},
{1, 11, 10, 1, 4, 11, 1, 0, 4, 7, 11, 4, -1},
{4, 7, 8, 9, 0, 11, 9, 11, 10, 11, 0, 3, -1},
{4, 7, 11, 4, 11, 9, 9, 11, 10, -1},
{9, 5, 4, -1},
{9, 5, 4, 0, 8, 3, -1},
{0, 5, 4, 1, 5, 0, -1},
{8, 5, 4, 8, 3, 5, 3, 1, 5, -1},
{1, 2, 10, 9, 5, 4, -1},
{3, 0, 8, 1, 2, 10, 4, 9, 5, -1},
{5, 2, 10, 5, 4, 2, 4, 0, 2, -1},
{2, 10, 5, 3, 2, 5, 3, 5, 4, 3, 4, 8, -1},
{9, 5, 4, 2, 3, 11, -1},
{0, 11, 2, 0, 8, 11, 4, 9, 5, -1},
{0, 5, 4, 0, 1, 5, 2, 3, 11, -1},
{2, 1, 5, 2, 5, 8, 2, 8, 11, 4, 8, 5, -1},
{10, 3, 11, 10, 1, 3, 9, 5, 4, -1},
{4, 9, 5, 0, 8, 1, 8, 10, 1, 8, 11, 10, -1},
{5, 4, 0, 5, 0, 11, 5, 11, 10, 11, 0, 3, -1},
{5, 4, 8, 5, 8, 10, 10, 8, 11, -1},
{9, 7, 8, 5, 7, 9, -1},
{9, 3, 0, 9, 5, 3, 5, 7, 3, -1},
{0, 7, 8, 0, 1, 7, 1, 5, 7, -1},
{1, 5, 3, 3, 5, 7, -1},
{9, 7, 8, 9, 5, 7, 10, 1, 2, -1},
{10, 1, 2, 9, 5, 0, 5, 3, 0, 5, 7, 3, -1},
{8, 0, 2, 8, 2, 5, 8, 5, 7, 10, 5, 2, -1},
{2, 10, 5, 2, 5, 3, 3, 5, 7, -1},
{7, 9, 5, 7, 8, 9, 3, 11, 2, -1},
{9, 5, 7, 9, 7, 2, 9, 2, 0, 2, 7, 11, -1},
{2, 3, 11, 0, 1, 8, 1, 7, 8, 1, 5, 7, -1},
{11, 2, 1, 11, 1, 7, 7, 1, 5, -1},
{9, 5, 8, 8, 5, 7, 10, 1, 3, 10, 3, 11, -1},
{5, 7, 0, 5, 0, 9, 7, 11, 0, 1, 0, 10, 11, 10, 0, -1},
{11, 10, 0, 11, 0, 3, 10, 5, 0, 8, 0, 7, 5, 7, 0, -1},
{11, 10, 5, 7, 11, 5, -1},
{10, 6, 5, -1},
{0, 8, 3, 5, 10, 6, -1},
{9, 0, 1, 5, 10, 6, -1},
{1, 8, 3, 1, 9, 8, 5, 10, 6, -1},
{1, 6, 5, 2, 6, 1, -1},
{1, 6, 5, 1, 2, 6, 3, 0, 8, -1},
{9, 6, 5, 9, 0, 6, 0, 2, 6, -1},
{5, 9, 8, 5, 8, 2, 5, 2, 6, 3, 2, 8, -1},
{2, 3, 11, 10, 6, 5, -1},
{11, 0, 8, 11, 2, 0, 10, 6, 5, -1},
{0, 1, 9, 2, 3, 11, 5, 10, 6, -1},
{5, 10, 6, 1, 9, 2, 9, 11, 2, 9, 8, 11, -1},
{6, 3, 11, 6, 5, 3, 5, 1, 3, -1},
{0, 8, 11, 0, 11, 5, 0, 5, 1, 5, 11, 6, -1},
{3, 11, 6, 0, 3, 6, 0, 6, 5, 0, 5, 9, -1},
{6, 5, 9, 6, 9, 11, 11, 9, 8, -1},
{5, 10, 6, 4, 7, 8, -1},
{4, 3, 0, 4, 7, 3, 6, 5, 10, -1},
{1, 9, 0, 5, 10, 6, 8, 4, 7, -1},
{10, 6, 5, 1, 9, 7, 1, 7, 3, 7, 9, 4, -1},
{6, 1, 2, 6, 5, 1, 4, 7, 8, -1},
{1, 2, 5, 5, 2, 6, 3, 0, 4, 3, 4, 7, -1},
{8, 4, 7, 9, 0, 5, 0, 6, 5, 0, 2, 6, -1},
{7, 3, 9, 7, 9, 4, 3, 2, 9, 5, 9, 6, 2, 6, 9, -1},
{3, 11, 2, 7, 8, 4, 10, 6, 5, -1},
{5, 10, 6, 4, 7, 2, 4, 2, 0, 2, 7, 11, -1},
{0, 1, 9, 4, 7, 8, 2, 3, 11, 5, 10, 6, -1},
{9, 2, 1, 9, 11, 2, 9, 4, 11, 7, 11, 4, 5, 10, 6, -1},
{8, 4, 7, 3, 11, 5, 3, 5, 1, 5, 11, 6, -1},
{5, 1, 11, 5, 11, 6, 1, 0, 11, 7, 11, 4, 0, 4, 11, -1},
{0, 5, 9, 0, 6, 5, 0, 3, 6, 11, 6, 3, 8, 4, 7, -1},
{6, 5, 9, 6, 9, 11, 4, 7, 9, 7, 11, 9, -1},
{10, 4, 9, 6, 4, 10, -1},
{4, 10, 6, 4, 9, 10, 0, 8, 3, -1},
{10, 0, 1, 10, 6, 0, 6, 4, 0, -1},
{8, 3, 1, 8, 1, 6, 8, 6, 4, 6, 1, 10, -1},
{1, 4, 9, 1, 2, 4, 2, 6, 4, -1},
{3, 0, 8, 1, 2, 9, 2, 4, 9, 2, 6, 4, -1},
{0, 2, 4, 4, 2, 6, -1},
{8, 3, 2, 8, 2, 4, 4, 2, 6, -1},
{10, 4, 9, 10, 6, 4, 11, 2, 3, -1},
{0, 8, 2, 2, 8, 11, 4, 9, 10, 4, 10, 6, -1},
{3, 11, 2, 0, 1, 6, 0, 6, 4, 6, 1, 10, -1},
{6, 4, 1, 6, 1, 10, 4, 8, 1, 2, 1, 11, 8, 11, 1, -1},
{9, 6, 4, 9, 3, 6, 9, 1, 3, 11, 6, 3, -1},
{8, 11, 1, 8, 1, 0, 11, 6, 1, 9, 1, 4, 6, 4, 1, -1},
{3, 11, 6, 3, 6, 0, 0, 6, 4, -1},
{6, 4, 8, 11, 6, 8, -1},
{7, 10, 6, 7, 8, 10, 8, 9, 10, -1},
{0, 7, 3, 0, 10, 7, 0, 9, 10, 6, 7, 10, -1},
{10, 6, 7, 1, 10, 7, 1, 7, 8, 1, 8, 0, -1},
{10, 6, 7, 10, 7, 1, 1, 7, 3, -1},
{1, 2, 6, 1, 6, 8, 1, 8, 9, 8, 6, 7, -1},
{2, 6, 9, 2, 9, 1, 6, 7, 9, 0, 9, 3, 7, 3, 9, -1},
{7, 8, 0, 7, 0, 6, 6, 0, 2, -1},
{7, 3, 2, 6, 7, 2, -1},
{2, 3, 11, 10, 6, 8, 10, 8, 9, 8, 6, 7, -1},
{2, 0, 7, 2, 7, 11, 0, 9, 7, 6, 7, 10, 9, 10, 7, -1},
{1, 8, 0, 1, 7, 8, 1, 10, 7, 6, 7, 10, 2, 3, 11, -1},
{11, 2, 1, 11, 1, 7, 10, 6, 1, 6, 7, 1, -1},
{8, 9, 6, 8, 6, 7, 9, 1, 6, 11, 6, 3, 1, 3, 6, -1},
{0, 9, 1, 11, 6, 7, -1},
{7, 8, 0, 7, 0, 6, 3, 11, 0, 11, 6, 0, -1},
{7, 11, 6, -1},
{7, 6, 11, -1},
{3, 0, 8, 11, 7, 6, -1},
{0, 1, 9, 11, 7, 6, -1},
{8, 1, 9, 8, 3, 1, 11, 7, 6, -1},
{10, 1, 2, 6, 11, 7, -1},
{1, 2, 10, 3, 0, 8, 6, 11, 7, -1},
{2, 9, 0, 2, 10, 9, 6, 11, 7, -1},
{6, 11, 7, 2, 10, 3, 10, 8, 3, 10, 9, 8, -1},
{7, 2, 3, 6, 2, 7, -1},
{7, 0, 8, 7, 6, 0, 6, 2, 0, -1},
{2, 7, 6, 2, 3, 7, 0, 1, 9, -1},
{1, 6, 2, 1, 8, 6, 1, 9, 8, 8, 7, 6, -1},
{10, 7, 6, 10, 1, 7, 1, 3, 7, -1},
{10, 7, 6, 1, 7, 10, 1, 8, 7, 1, 0, 8, -1},
{0, 3, 7, 0, 7, 10, 0, 10, 9, 6, 10, 7, -1},
{7, 6, 10, 7, 10, 8, 8, 10, 9, -1},
{6, 8, 4, 11, 8, 6, -1},
{3, 6, 11, 3, 0, 6, 0, 4, 6, -1},
{8, 6, 11, 8, 4, 6, 9, 0, 1, -1},
{9, 4, 6, 9, 6, 3, 9, 3, 1, 11, 3, 6, -1},
{6, 8, 4, 6, 11, 8, 2, 10, 1, -1},
{1, 2, 10, 3, 0, 11, 0, 6, 11, 0, 4, 6, -1},
{4, 11, 8, 4, 6, 11, 0, 2, 9, 2, 10, 9, -1},
{10, 9, 3, 10, 3, 2, 9, 4, 3, 11, 3, 6, 4, 6, 3, -1},
{8, 2, 3, 8, 4, 2, 4, 6, 2, -1},
{0, 4, 2, 4, 6, 2, -1},
{1, 9, 0, 2, 3, 4, 2, 4, 6, 4, 3, 8, -1},
{1, 9, 4, 1, 4, 2, 2, 4, 6, -1},
{8, 1, 3, 8, 6, 1, 8, 4, 6, 6, 10, 1, -1},
{10, 1, 0, 10, 0, 6, 6, 0, 4, -1},
{4, 6, 3, 4, 3, 8, 6, 10, 3, 0, 3, 9, 10, 9, 3, -1},
{10, 9, 4, 6, 10, 4, -1},
{4, 9, 5, 7, 6, 11, -1},
{0, 8, 3, 4, 9, 5, 11, 7, 6, -1},
{5, 0, 1, 5, 4, 0, 7, 6, 11, -1},
{11, 7, 6, 8, 3, 4, 3, 5, 4, 3, 1, 5, -1},
{9, 5, 4, 10, 1, 2, 7, 6, 11, -1},
{6, 11, 7, 1, 2, 10, 0, 8, 3, 4, 9, 5, -1},
{7, 6, 11, 5, 4, 10, 4, 2, 10, 4, 0, 2, -1},
{3, 4, 8, 3, 5, 4, 3, 2, 5, 10, 5, 2, 11, 7, 6, -1},
{7, 2, 3, 7, 6, 2, 5, 4, 9, -1},
{9, 5, 4, 0, 8, 6, 0, 6, 2, 6, 8, 7, -1},
{3, 6, 2, 3, 7, 6, 1, 5, 0, 5, 4, 0, -1},
{6, 2, 8, 6, 8, 7, 2, 1, 8, 4, 8, 5, 1, 5, 8, -1},
{9, 5, 4, 10, 1, 6, 1, 7, 6, 1, 3, 7, -1},
{1, 6, 10, 1, 7, 6, 1, 0, 7, 8, 7, 0, 9, 5, 4, -1},
{4, 0, 10, 4, 10, 5, 0, 3, 10, 6, 10, 7, 3, 7, 10, -1},
{7, 6, 10, 7, 10, 8, 5, 4, 10, 4, 8, 10, -1},
{6, 9, 5, 6, 11, 9, 11, 8, 9, -1},
{3, 6, 11, 0, 6, 3, 0, 5, 6, 0, 9, 5, -1},
{0, 11, 8, 0, 5, 11, 0, 1, 5, 5, 6, 11, -1},
{6, 11, 3, 6, 3, 5, 5, 3, 1, -1},
{1, 2, 10, 9, 5, 11, 9, 11, 8, 11, 5, 6, -1},
{0, 11, 3, 0, 6, 11, 0, 9, 6, 5, 6, 9, 1, 2, 10, -1},
{11, 8, 5, 11, 5, 6, 8, 0, 5, 10, 5, 2, 0, 2, 5, -1},
{6, 11, 3, 6, 3, 5, 2, 10, 3, 10, 5, 3, -1},
{5, 8, 9, 5, 2, 8, 5, 6, 2, 3, 8, 2, -1},
{9, 5, 6, 9, 6, 0, 0, 6, 2, -1},
{1, 5, 8, 1, 8, 0, 5, 6, 8, 3, 8, 2, 6, 2, 8, -1},
{1, 5, 6, 2, 1, 6, -1},
{1, 3, 6, 1, 6, 10, 3, 8, 6, 5, 6, 9, 8, 9, 6, -1},
{10, 1, 0, 10, 0, 6, 9, 5, 0, 5, 6, 0, -1},
{0, 3, 8, 5, 6, 10, -1},
{10, 5, 6, -1},
{11, 5, 10, 7, 5, 11, -1},
{11, 5, 10, 11, 7, 5, 8, 3, 0, -1},
{5, 11, 7, 5, 10, 11, 1, 9, 0, -1},
{10, 7, 5, 10, 11, 7, 9, 8, 1, 8, 3, 1, -1},
{11, 1, 2, 11, 7, 1, 7, 5, 1, -1},
{0, 8, 3, 1, 2, 7, 1, 7, 5, 7, 2, 11, -1},
{9, 7, 5, 9, 2, 7, 9, 0, 2, 2, 11, 7, -1},
{7, 5, 2, 7, 2, 11, 5, 9, 2, 3, 2, 8, 9, 8, 2, -1},
{2, 5, 10, 2, 3, 5, 3, 7, 5, -1},
{8, 2, 0, 8, 5, 2, 8, 7, 5, 10, 2, 5, -1},
{9, 0, 1, 5, 10, 3, 5, 3, 7, 3, 10, 2, -1},
{9, 8, 2, 9, 2, 1, 8, 7, 2, 10, 2, 5, 7, 5, 2, -1},
{1, 3, 5, 3, 7, 5, -1},
{0, 8, 7, 0, 7, 1, 1, 7, 5, -1},
{9, 0, 3, 9, 3, 5, 5, 3, 7, -1},
{9, 8, 7, 5, 9, 7, -1},
{5, 8, 4, 5, 10, 8, 10, 11, 8, -1},
{5, 0, 4, 5, 11, 0, 5, 10, 11, 11, 3, 0, -1},
{0, 1, 9, 8, 4, 10, 8, 10, 11, 10, 4, 5, -1},
{10, 11, 4, 10, 4, 5, 11, 3, 4, 9, 4, 1, 3, 1, 4, -1},
{2, 5, 1, 2, 8, 5, 2, 11, 8, 4, 5, 8, -1},
{0, 4, 11, 0, 11, 3, 4, 5, 11, 2, 11, 1, 5, 1, 11, -1},
{0, 2, 5, 0, 5, 9, 2, 11, 5, 4, 5, 8, 11, 8, 5, -1},
{9, 4, 5, 2, 11, 3, -1},
{2, 5, 10, 3, 5, 2, 3, 4, 5, 3, 8, 4, -1},
{5, 10, 2, 5, 2, 4, 4, 2, 0, -1},
{3, 10, 2, 3, 5, 10, 3, 8, 5, 4, 5, 8, 0, 1, 9, -1},
{5, 10, 2, 5, 2, 4, 1, 9, 2, 9, 4, 2, -1},
{8, 4, 5, 8, 5, 3, 3, 5, 1, -1},
{0, 4, 5, 1, 0, 5, -1},
{8, 4, 5, 8, 5, 3, 9, 0, 5, 0, 3, 5, -1},
{9, 4, 5, -1},
{4, 11, 7, 4, 9, 11, 9, 10, 11, -1},
{0, 8, 3, 4, 9, 7, 9, 11, 7, 9, 10, 11, -1},
{1, 10, 11, 1, 11, 4, 1, 4, 0, 7, 4, 11, -1},
{3, 1, 4, 3, 4, 8, 1, 10, 4, 7, 4, 11, 10, 11, 4, -1},
{4, 11, 7, 9, 11, 4, 9, 2, 11, 9, 1, 2, -1},
{9, 7, 4, 9, 11, 7, 9, 1, 11, 2, 11, 1, 0, 8, 3, -1},
{11, 7, 4, 11, 4, 2, 2, 4, 0, -1},
{11, 7, 4, 11, 4, 2, 8, 3, 4, 3, 2, 4, -1},
{2, 9, 10, 2, 7, 9, 2, 3, 7, 7, 4, 9, -1},
{9, 10, 7, 9, 7, 4, 10, 2, 7, 8, 7, 0, 2, 0, 7, -1},
{3, 7, 10, 3, 10, 2, 7, 4, 10, 1, 10, 0, 4, 0, 10, -1},
{1, 10, 2, 8, 7, 4, -1},
{4, 9, 1, 4, 1, 7, 7, 1, 3, -1},
{4, 9, 1, 4, 1, 7, 0, 8, 1, 8, 7, 1, -1},
{4, 0, 3, 7, 4, 3, -1},
{4, 8, 7, -1},
{9, 10, 8, 10, 11, 8, -1},
{3, 0, 9, 3, 9, 11, 11, 9, 10, -1},
{0, 1, 10, 0, 10, 8, 8, 10, 11, -1},
{3, 1, 10, 11, 3, 10, -1},
{1, 2, 11, 1, 11, 9, 9, 11, 8, -1},
{3, 0, 9, 3, 9, 11, 1, 2, 9, 2, 11, 9, -1},
{0, 2, 11, 8, 0, 11, -1},
{3, 2, 11, -1},
{2, 3, 8, 2, 8, 10, 10, 8, 9, -1},
{9, 10, 2, 0, 9, 2, -1},
{2, 3, 8, 2, 8, 10, 0, 1, 8, 1, 10, 8, -1},
{1, 10, 2, -1},
{1, 3, 8, 9, 1, 8, -1},
{0, 9, 1, -1},
{0, 3, 8, -1},
{-1}
};

static float hash3(int x, int y, int z) {
    uint32_t h = static_cast<uint32_t>(x * 374761393u + y * 668265263u + z * 2147483647u);
    h = (h ^ (h >> 13)) * 1274126177u;
    return static_cast<float>(h & 0x00FFFFFF) / static_cast<float>(0x00FFFFFF);
}

static float valueNoise3D(float x, float y, float z) {
    int ix = static_cast<int>(std::floor(x));
    int iy = static_cast<int>(std::floor(y));
    int iz = static_cast<int>(std::floor(z));
    float fx = x - ix;
    float fy = y - iy;
    float fz = z - iz;

    auto smooth = [](float t) { return t * t * (3.0f - 2.0f * t); };
    float u = smooth(fx);
    float v = smooth(fy);
    float w = smooth(fz);

    float n000 = hash3(ix, iy, iz);
    float n100 = hash3(ix + 1, iy, iz);
    float n010 = hash3(ix, iy + 1, iz);
    float n110 = hash3(ix + 1, iy + 1, iz);
    float n001 = hash3(ix, iy, iz + 1);
    float n101 = hash3(ix + 1, iy, iz + 1);
    float n011 = hash3(ix, iy + 1, iz + 1);
    float n111 = hash3(ix + 1, iy + 1, iz + 1);

    float nx00 = glm::mix(n000, n100, u);
    float nx10 = glm::mix(n010, n110, u);
    float nx01 = glm::mix(n001, n101, u);
    float nx11 = glm::mix(n011, n111, u);

    float nxy0 = glm::mix(nx00, nx10, v);
    float nxy1 = glm::mix(nx01, nx11, v);

    return glm::mix(nxy0, nxy1, w) * 2.0f - 1.0f;
}
}

ExplosionVolumeSystem::~ExplosionVolumeSystem() {
    for (auto& v : volumes) {
        v.mesh.destroy();
    }
}

void ExplosionVolumeSystem::spawn(const glm::vec3& center, float power) {
    ExplosionVolume volume;
    volume.center = center;
    volume.power = power;
    volume.duration = 2.4f + power * 0.2f;
    volume.radius = power * 2.2f;
    volume.gridSize = (power <= 4.0f) ? 22 : 26;
    volume.isoLevel = 0.1f;
    volume.rebuildTimer = 0.0f;

    std::random_device rd;
    volume.seed = static_cast<float>(rd() % 10000) * 0.01f;

    buildMesh(volume);
    volumes.push_back(std::move(volume));
}

void ExplosionVolumeSystem::update(float deltaTime) {
    for (auto& v : volumes) {
        v.age += deltaTime;
    }

    volumes.erase(std::remove_if(volumes.begin(), volumes.end(), [](ExplosionVolume& v) {
        if (v.age >= v.duration) {
            v.mesh.destroy();
            return true;
        }
        return false;
    }), volumes.end());
}

void ExplosionVolumeSystem::render(Shader& shader, const glm::mat4& view, const glm::mat4& projection,
                                   const glm::vec3& cameraPos, const glm::vec3& lightDir,
                                   const glm::vec3& renderOrigin) {
    if (volumes.empty()) return;

    shader.use();
    shader.setMat4("uView", view);
    shader.setMat4("uProjection", projection);
    shader.setVec3("uCameraPos", cameraPos);
    shader.setVec3("uLightDir", glm::normalize(lightDir));
    shader.setVec3("uRenderOrigin", renderOrigin);

    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glDisable(GL_CULL_FACE);
    glDepthMask(GL_FALSE);

    for (auto& v : volumes) {
        if (!v.meshReady || v.mesh.indexCount == 0) continue;
        shader.setVec3("uCenter", v.center);
        shader.setFloat("uAge", v.age);
        shader.setFloat("uDuration", v.duration);
        shader.setFloat("uRadius", v.radius);
        float t = glm::clamp(v.age / v.duration, 0.0f, 1.0f);
        float expand = 0.7f + 0.6f * (1.0f - std::exp(-v.age * 2.5f));
        float scale = glm::mix(0.7f, 1.25f, expand) * (1.0f + t * 0.1f);
        shader.setFloat("uScale", scale);
        shader.setFloat("uNoisePhase", v.seed + v.age * 1.6f);
        v.mesh.draw();
    }

    glDepthMask(GL_TRUE);
    glEnable(GL_CULL_FACE);
    glDisable(GL_BLEND);
    shader.unuse();
}

void ExplosionVolumeSystem::ExplosionMesh::upload(const std::vector<ExplosionVertex>& vertices,
                                                  const std::vector<uint32_t>& indices) {
    if (vao == 0) {
        glGenVertexArrays(1, &vao);
        glGenBuffers(1, &vbo);
        glGenBuffers(1, &ebo);
    }

    glBindVertexArray(vao);
    glBindBuffer(GL_ARRAY_BUFFER, vbo);
    glBufferData(GL_ARRAY_BUFFER, vertices.size() * sizeof(ExplosionVertex), vertices.data(), GL_DYNAMIC_DRAW);

    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, ebo);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, indices.size() * sizeof(uint32_t), indices.data(), GL_DYNAMIC_DRAW);

    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(ExplosionVertex), (void*)offsetof(ExplosionVertex, position));
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, sizeof(ExplosionVertex), (void*)offsetof(ExplosionVertex, normal));
    glEnableVertexAttribArray(2);
    glVertexAttribPointer(2, 4, GL_FLOAT, GL_FALSE, sizeof(ExplosionVertex), (void*)offsetof(ExplosionVertex, color));

    glBindVertexArray(0);

    indexCount = indices.size();
}

void ExplosionVolumeSystem::ExplosionMesh::draw() const {
    if (vao == 0 || indexCount == 0) return;
    glBindVertexArray(vao);
    glDrawElements(GL_TRIANGLES, static_cast<GLsizei>(indexCount), GL_UNSIGNED_INT, 0);
    glBindVertexArray(0);
}

void ExplosionVolumeSystem::ExplosionMesh::destroy() {
    if (ebo) glDeleteBuffers(1, &ebo);
    if (vbo) glDeleteBuffers(1, &vbo);
    if (vao) glDeleteVertexArrays(1, &vao);
    vao = vbo = ebo = 0;
    indexCount = 0;
}

float ExplosionVolumeSystem::sampleDensity(const ExplosionVolume& volume, const glm::vec3& p) const {
    float t = glm::clamp(volume.age / volume.duration, 0.0f, 1.0f);
    float expand = 0.6f + 0.6f * (1.0f - std::exp(-volume.age * 2.5f));
    float radius = volume.radius * expand;

    glm::vec3 local = p - volume.center;
    float dist = glm::length(local);
    float base = (radius - dist) / radius;
    base = glm::clamp(base, 0.0f, 1.0f);

    float noise = 0.0f;
    float freq = 0.18f;
    float amp = 0.6f;
    glm::vec3 npos = local * freq + glm::vec3(0.0f, volume.age * 0.8f, 0.0f) + volume.seed;
    for (int i = 0; i < 3; ++i) {
        noise += valueNoise3D(npos.x, npos.y, npos.z) * amp;
        npos *= 2.1f;
        amp *= 0.5f;
    }

    float density = base * 1.6f + noise * 0.35f;
    density *= (1.0f - t) * 1.4f;
    return density;
}

glm::vec3 ExplosionVolumeSystem::sampleGradient(const ExplosionVolume& volume, const glm::vec3& p) const {
    const float e = 0.1f;
    float dx = sampleDensity(volume, p + glm::vec3(e, 0, 0)) - sampleDensity(volume, p - glm::vec3(e, 0, 0));
    float dy = sampleDensity(volume, p + glm::vec3(0, e, 0)) - sampleDensity(volume, p - glm::vec3(0, e, 0));
    float dz = sampleDensity(volume, p + glm::vec3(0, 0, e)) - sampleDensity(volume, p - glm::vec3(0, 0, e));
    glm::vec3 n(dx, dy, dz);
    if (glm::length(n) < 0.0001f) return glm::vec3(0, 1, 0);
    return glm::normalize(n);
}

void ExplosionVolumeSystem::buildMesh(ExplosionVolume& volume) {
    std::vector<ExplosionVertex> vertices;
    std::vector<uint32_t> indices;

    int N = volume.gridSize;
    float t = glm::clamp(volume.age / volume.duration, 0.0f, 1.0f);
    float expand = 0.6f + 0.6f * (1.0f - std::exp(-volume.age * 2.5f));
    float radius = volume.radius * expand;
    float cell = (radius * 2.0f) / static_cast<float>(N - 1);
    glm::vec3 origin = volume.center - glm::vec3(radius);

    for (int z = 0; z < N - 1; ++z) {
        for (int y = 0; y < N - 1; ++y) {
            for (int x = 0; x < N - 1; ++x) {
                glm::vec3 p[8];
                float val[8];

                for (int i = 0; i < 8; ++i) {
                    int ix = x + ((i & 1) ? 1 : 0);
                    int iy = y + ((i & 2) ? 1 : 0);
                    int iz = z + ((i & 4) ? 1 : 0);
                    p[i] = origin + glm::vec3(ix * cell, iy * cell, iz * cell);
                    val[i] = sampleDensity(volume, p[i]);
                }

                int cubeIndex = 0;
                if (val[0] > volume.isoLevel) cubeIndex |= 1;
                if (val[1] > volume.isoLevel) cubeIndex |= 2;
                if (val[2] > volume.isoLevel) cubeIndex |= 4;
                if (val[3] > volume.isoLevel) cubeIndex |= 8;
                if (val[4] > volume.isoLevel) cubeIndex |= 16;
                if (val[5] > volume.isoLevel) cubeIndex |= 32;
                if (val[6] > volume.isoLevel) cubeIndex |= 64;
                if (val[7] > volume.isoLevel) cubeIndex |= 128;

                if (edgeTable[cubeIndex] == 0) continue;

                glm::vec3 vertList[12];

                auto interp = [&](int a, int b) {
                    float v1 = val[a];
                    float v2 = val[b];
                    float t = (volume.isoLevel - v1) / (v2 - v1 + 1e-6f);
                    return glm::mix(p[a], p[b], t);
                };

                if (edgeTable[cubeIndex] & 1) vertList[0] = interp(0, 1);
                if (edgeTable[cubeIndex] & 2) vertList[1] = interp(1, 2);
                if (edgeTable[cubeIndex] & 4) vertList[2] = interp(2, 3);
                if (edgeTable[cubeIndex] & 8) vertList[3] = interp(3, 0);
                if (edgeTable[cubeIndex] & 16) vertList[4] = interp(4, 5);
                if (edgeTable[cubeIndex] & 32) vertList[5] = interp(5, 6);
                if (edgeTable[cubeIndex] & 64) vertList[6] = interp(6, 7);
                if (edgeTable[cubeIndex] & 128) vertList[7] = interp(7, 4);
                if (edgeTable[cubeIndex] & 256) vertList[8] = interp(0, 4);
                if (edgeTable[cubeIndex] & 512) vertList[9] = interp(1, 5);
                if (edgeTable[cubeIndex] & 1024) vertList[10] = interp(2, 6);
                if (edgeTable[cubeIndex] & 2048) vertList[11] = interp(3, 7);

                for (int i = 0; triTable[cubeIndex][i] != -1; i += 3) {
                    if (indices.size() > 80000) break; // safety cap
                    int a0 = triTable[cubeIndex][i];
                    int a1 = triTable[cubeIndex][i + 1];
                    int a2 = triTable[cubeIndex][i + 2];

                    glm::vec3 pa = vertList[a0];
                    glm::vec3 pb = vertList[a1];
                    glm::vec3 pc = vertList[a2];

                    glm::vec3 na = sampleGradient(volume, pa);
                    glm::vec3 nb = sampleGradient(volume, pb);
                    glm::vec3 nc = sampleGradient(volume, pc);

                    float heat = glm::clamp(1.0f - t * 1.1f, 0.0f, 1.0f);
                    float smoke = glm::clamp(t * 1.1f, 0.0f, 1.0f);
                    float height = glm::clamp((pa.y - volume.center.y) / radius + 0.5f, 0.0f, 1.0f);

                    glm::vec4 color = glm::mix(glm::vec4(1.0f, 0.45f, 0.15f, 0.85f),
                                               glm::vec4(0.35f, 0.35f, 0.35f, 0.55f),
                                               smoke);
                    glm::vec3 rgb = glm::vec3(color) + heat * glm::vec3(0.7f, 0.25f, 0.05f);
                    color = glm::vec4(rgb, color.a);
                    color.a *= glm::mix(1.0f, 0.8f, height);

                    uint32_t baseIndex = static_cast<uint32_t>(vertices.size());
                    vertices.push_back({pa, na, color});
                    vertices.push_back({pb, nb, color});
                    vertices.push_back({pc, nc, color});

                    indices.push_back(baseIndex);
                    indices.push_back(baseIndex + 1);
                    indices.push_back(baseIndex + 2);
                }
                if (indices.size() > 80000) break;
            }
            if (indices.size() > 80000) break;
        }
        if (indices.size() > 80000) break;
    }

    volume.mesh.upload(vertices, indices);
    volume.meshReady = !indices.empty();
}
