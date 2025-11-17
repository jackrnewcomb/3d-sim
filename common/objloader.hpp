#pragma once

#include <fstream>
#include <iostream>
#include <sstream>
#include <string>
#include <vector>

static inline int fixIndex(int idx, int count)
{
    // Positive indices: OBJ is 1-based
    if (idx > 0)
        return idx - 1;

    // Negative indices: relative to end
    if (idx < 0)
        return count + idx;

    return -1; // shouldn't happen
}

bool loadOBJ(const char *path, std::vector<float> &out_vertices, std::vector<float> &out_uvs,
             std::vector<float> &out_normals)
{
    std::ifstream file(path);
    if (!file.is_open())
    {
        std::cerr << "Failed to open OBJ: " << path << "\n";
        return false;
    }

    // temporary storage
    std::vector<float> temp_v;
    std::vector<float> temp_vt;
    std::vector<float> temp_vn;

    struct Idx
    {
        int v, t, n;
    };
    std::vector<Idx> faces;

    std::string line;
    while (std::getline(file, line))
    {
        std::stringstream ss(line);
        std::string type;
        ss >> type;

        if (type == "v")
        {
            float x, y, z;
            ss >> x >> y >> z;
            temp_v.insert(temp_v.end(), {x, y, z});
        }
        else if (type == "vt")
        {
            float u, v;
            ss >> u >> v;
            temp_vt.insert(temp_vt.end(), {u, v});
        }
        else if (type == "vn")
        {
            float x, y, z;
            ss >> x >> y >> z;
            temp_vn.insert(temp_vn.end(), {x, y, z});
        }
        else if (type == "f")
        {
            for (int i = 0; i < 3; i++)
            {
                std::string tok;
                ss >> tok;

                int vi, ti, ni;
                sscanf(tok.c_str(), "%d/%d/%d", &vi, &ti, &ni);

                Idx idx;
                idx.v = fixIndex(vi, temp_v.size() / 3);
                idx.t = fixIndex(ti, temp_vt.size() / 2);
                idx.n = fixIndex(ni, temp_vn.size() / 3);

                faces.push_back(idx);
            }
        }
    }

    // Build final unrolled arrays
    for (auto &f : faces)
    {
        // vertices
        out_vertices.push_back(temp_v[f.v * 3 + 0]);
        out_vertices.push_back(temp_v[f.v * 3 + 1]);
        out_vertices.push_back(temp_v[f.v * 3 + 2]);

        // UVs
        out_uvs.push_back(temp_vt[f.t * 2 + 0]);
        out_uvs.push_back(temp_vt[f.t * 2 + 1]);

        // normals
        out_normals.push_back(temp_vn[f.n * 3 + 0]);
        out_normals.push_back(temp_vn[f.n * 3 + 1]);
        out_normals.push_back(temp_vn[f.n * 3 + 2]);
    }

    return true;
}
