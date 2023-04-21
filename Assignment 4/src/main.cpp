////////////////////////////////////////////////////////////////////////////////
// C++ include
#include <iostream>
#include <string>
#include <vector>
#include <limits>
#include <fstream>
#include <algorithm>
#include <numeric>

// Utilities for the Assignment
#include "utils.h"

// Image writing library
#define STB_IMAGE_WRITE_IMPLEMENTATION // Do not include this line twice in your project!
#include "stb_image_write.h"

// Shortcut to avoid Eigen:: everywhere, DO NOT USE IN .h
using namespace Eigen;

////////////////////////////////////////////////////////////////////////////////
// Class to store tree
////////////////////////////////////////////////////////////////////////////////
class AABBTree
{
public:
    class Node
    {
    public:
        AlignedBox3d bbox;
        int parent;   // Index of the parent node (-1 for root)
        int left;     // Index of the left child (-1 for a leaf)
        int right;    // Index of the right child (-1 for a leaf)
        int triangle; // Index of the node triangle (-1 for internal nodes)
    };

    std::vector<Node> nodes;
    int root;

    AABBTree() = default;                           // Default empty constructor
    AABBTree(const MatrixXd &V, const MatrixXi &F); // Build a BVH from an existing mesh
};

////////////////////////////////////////////////////////////////////////////////
// Scene setup, global variables
////////////////////////////////////////////////////////////////////////////////
std::vector<Vector3d> sphere_centers; // sphere center
std::vector<double> sphere_radii;     // sphere radius
std::vector<Matrix3d> parallelograms;
const int max_bounce = 5; // derived from a3

const std::string data_dir = DATA_DIR;
const std::string filename("raytrace.png"); // output png
const std::string mesh_filename(data_dir + "bunny.off"); // read the file


//Camera settings
const double focal_length = 2;
const double field_of_view = 0.7854; //45 degrees
const bool is_perspective = true;
const Vector3d camera_position(0, 0, 2);

// Triangle Mesh
MatrixXd vertices; // n x 3 matrix (n points)
MatrixXi facets;   // m x 3 matrix (m triangles)
AABBTree bvh;

//Material for the object, same material for all objects
const Vector4d obj_ambient_color(0.0, 0.5, 0.0, 0);
const Vector4d obj_diffuse_color(0.5, 0.5, 0.5, 0);
const Vector4d obj_specular_color(0.2, 0.2, 0.2, 0);
const double obj_specular_exponent = 256.0;
const Vector4d obj_reflection_color(0.7, 0.7, 0.7, 0);

// Precomputed (or otherwise) gradient vectors at each grid node
const int grid_size = 20;
std::vector<std::vector<Vector2d>> grid;

//Lights
std::vector<Vector3d> light_positions;
std::vector<Vector4d> light_colors;
//Ambient light
const Vector4d ambient_light(0.2, 0.2, 0.2, 0);

//Fills the different arrays
void setup_scene()
{
    //Loads file
    std::ifstream in(mesh_filename);
    std::string token;
    in >> token;
    int nv, nf, ne;
    in >> nv >> nf >> ne;
    vertices.resize(nv, 3);
    facets.resize(nf, 3);
    for (int i = 0; i < nv; ++i)
    {
        in >> vertices(i, 0) >> vertices(i, 1) >> vertices(i, 2);
    }
    for (int i = 0; i < nf; ++i)
    {
        int s;
        in >> s >> facets(i, 0) >> facets(i, 1) >> facets(i, 2);
        assert(s == 3);
    }

    //setup tree
    bvh = AABBTree(vertices, facets);

    //Lights
    light_positions.emplace_back(8, 8, 0);
    light_colors.emplace_back(16, 16, 16, 0);

    light_positions.emplace_back(6, -8, 0);
    light_colors.emplace_back(16, 16, 16, 0);

    light_positions.emplace_back(4, 8, 0);
    light_colors.emplace_back(16, 16, 16, 0);

    light_positions.emplace_back(2, -8, 0);
    light_colors.emplace_back(16, 16, 16, 0);

    light_positions.emplace_back(0, 8, 0);
    light_colors.emplace_back(16, 16, 16, 0);

    light_positions.emplace_back(-2, -8, 0);
    light_colors.emplace_back(16, 16, 16, 0);

    light_positions.emplace_back(-4, 8, 0);
    light_colors.emplace_back(16, 16, 16, 0);

    //Spheres
    sphere_centers.emplace_back(10, 0, 1);
    sphere_radii.emplace_back(1);

    sphere_centers.emplace_back(7, 0.05, -1);
    sphere_radii.emplace_back(1);

    sphere_centers.emplace_back(4, 0.1, 1);
    sphere_radii.emplace_back(1);

    sphere_centers.emplace_back(1, 0.2, -1);
    sphere_radii.emplace_back(1);

    sphere_centers.emplace_back(-2, 0.4, 1);
    sphere_radii.emplace_back(1);

    sphere_centers.emplace_back(-5, 0.8, -1);
    sphere_radii.emplace_back(1);

    sphere_centers.emplace_back(-8, 1.6, 1);
    sphere_radii.emplace_back(1);

    //parallelograms
    parallelograms.emplace_back();
    parallelograms.back() << -100, 100, -100,
        -1.25, 0, -1.2,
        -100, -100, 100;
}

////////////////////////////////////////////////////////////////////////////////
// BVH Code
////////////////////////////////////////////////////////////////////////////////

AlignedBox3d bbox_from_triangle(const Vector3d &a, const Vector3d &b, const Vector3d &c)
{
    AlignedBox3d box;
    box.extend(a);
    box.extend(b);
    box.extend(c);
    return box;
}

AABBTree::AABBTree(const MatrixXd &V, const MatrixXi &F)
{
    // Compute the centroids of all the triangles in the input mesh
    MatrixXd centroids(F.rows(), V.cols());
    centroids.setZero();
    for (int i = 0; i < F.rows(); ++i)
    {
        for (int k = 0; k < F.cols(); ++k)
        {
            centroids.row(i) += V.row(F(i, k));
        }
        centroids.row(i) /= F.cols();
    }

    // TODO

    // Split each set of primitives into 2 sets of roughly equal size,
    // based on sorting the centroids along one direction or another.
}

////////////////////////////////////////////////////////////////////////////////
// Intersection code
////////////////////////////////////////////////////////////////////////////////

double ray_triangle_intersection(const Vector3d &ray_origin, const Vector3d &ray_direction, const Vector3d &a, const Vector3d &b, const Vector3d &c, Vector3d &p, Vector3d &N)
{
    // TODO
    // Compute whether the ray intersects the given triangle.
    // If you have done the parallelogram case, this should be very similar to it.
    const Vector3d tri_origin = a;
    const Vector3d pgram_u = b - tri_origin;
    const Vector3d pgram_v = c - tri_origin;

    Matrix3d scale;// formula for ray-tracing arallelogram to check if it intersect
            scale <<pgram_u[0],pgram_v[0],-ray_direction[0],
                    pgram_u[1],pgram_v[1],-ray_direction[1],
                    pgram_u[2],pgram_v[2],-ray_direction[2];
            
    Vector3d y_position = ray_origin - tri_origin;
    Vector3d radius = scale.inverse()*y_position;

        if (0<=radius[0] && radius[0]<=1 && 0<=radius[1] && radius[1]<=1 && (radius[0]+radius[1]) <= 1){
        //Vector3d ray_intersection = ray_origin + ray_direction * radius[2];
        // TODO: Compute normal at the intersection point
            double x = (-1)*radius[2];
            p = ray_origin + ray_direction*(-1)*(x);
            Vector3d normal_vector = (-1)*pgram_u.cross(pgram_v);  // reverse the direction
            N= normal_vector.normalized();
        return -x;
     }
    return -1;
   
}
// code from a3
double ray_sphere_intersection(const Vector3d &ray_origin, const Vector3d &ray_direction, int index, Vector3d &p, Vector3d &N)
{
    // TODO, implement the intersection between the ray and the sphere at index index.
    //return t or -1 if no intersection

    const Vector3d sphere_center = sphere_centers[index];
    const double sphere_radius = sphere_radii[index];
    double delta = 4 * pow((ray_origin - sphere_center).dot(ray_direction), 2) - 4 * ray_direction.squaredNorm() * ((ray_origin - sphere_center).squaredNorm() - sphere_radius * sphere_radius);

    if (delta<0)
    {
        return -1;
    }
     //TODO set the correct intersection point, update p to the correct value
        //p = ray_origin;
        //N = ray_direction;
        double t = (-2 * (ray_origin - sphere_center).dot(ray_direction) - sqrt(delta)) / (2 * ray_direction.squaredNorm());
        
        p = ray_origin + t * ray_direction;
        N = (p-sphere_center).normalized();
      
        return t;
}

//Compute the intersection between a ray and a paralleogram, return -1 if no intersection
//code from a3
double ray_parallelogram_intersection(const Vector3d &ray_origin, const Vector3d &ray_direction, int index, Vector3d &p, Vector3d &N)
{
    // TODO, implement the intersection between the ray and the parallelogram at index index.
    //return t or -1 if no intersection

    const Vector3d pgram_origin = parallelograms[index].col(0);
    const Vector3d A = parallelograms[index].col(1);
    const Vector3d B = parallelograms[index].col(2);
    const Vector3d pgram_u = A - pgram_origin;
    const Vector3d pgram_v = B - pgram_origin;
            
    Matrix3d scale;// formula for ray-tracing arallelogram to check if it intersect
            scale <<pgram_u[0],pgram_v[0],-ray_direction[0],pgram_u[1],pgram_v[1],-ray_direction[1],
            pgram_u[2],pgram_v[2],-ray_direction[2];
            
            Vector3d y_position = ray_origin - pgram_origin;
            Vector3d radius = scale.inverse()*y_position;
    
    if (0<=radius[0] && radius[0]<=1 && 0<=radius[1] && radius[1]<=1){
        //Vector3d ray_intersection = ray_origin + ray_direction * radius[2];
        // TODO: Compute normal at the intersection point
            double x = (-1)*radius[2];
            p = ray_origin + ray_direction*(-1)*(x);
            Vector3d normal_vector = (-1)*pgram_u.cross(pgram_v);  // reverse the direction
            N= normal_vector.normalized();
        return -x;
    }
    return -1;
}

bool ray_box_intersection(const Vector3d &ray_origin, const Vector3d &ray_direction, const AlignedBox3d &box)
{
    // TODO
    // Compute whether the ray intersects the given box.
    // we are not testing with the real surface here anyway.
    return false;
}

//Finds the closest intersecting object returns its index
//In case of intersection it writes into p and N (intersection point and normals)
bool find_nearest_object(const Vector3d &ray_origin, const Vector3d &ray_direction, Vector3d &p, Vector3d &N)
{
    int closest_index = -1;
    double closest_t = std::numeric_limits<double>::max();

    Vector3d tmp_p, tmp_N;
    for (int i = 0; i < facets.rows();i++){
       
        Vector3d X,Y,Z;
        X[0] = vertices(facets(i,0),0);
        Y[0] = vertices(facets(i,1),0);
        Z[0] = vertices(facets(i,2),0);
        
        X[1] = vertices(facets(i,0),1);
        Y[1] = vertices(facets(i,1),1);
        Z[1] = vertices(facets(i,2),1);
        
        X[2] = vertices(facets(i,0),2);
        Y[2] = vertices(facets(i,1),2);
        Z[2] = vertices(facets(i,2),2);

         const double t = ray_triangle_intersection(ray_origin, ray_direction, X, Y, Z, tmp_p, tmp_N);
        //We have intersection
        if (t >= 0)
        {
            //The point is before our current closest t
            if (t < closest_t)
            {
                closest_index = i;
                closest_t = t;
                p = tmp_p;
                N = tmp_N;
            }
        }
    }
    for (int i = 0; i < sphere_centers.size(); i++){
        // returns t and writes on tmp_p and tmp_N
        //code from a3
        const double t = ray_sphere_intersection(ray_origin, ray_direction, i, tmp_p, tmp_N);
        // We have intersection
        if (t >= 0)
        {
            // The point is before our current closest t
            if (t < closest_t)
            {
                closest_index = i;
                closest_t = t;
                p = tmp_p;
                N = tmp_N;
            }
        }
        
    }
    for (int i = 0; i < parallelograms.size(); i++){
        //code from a3
        // returns t and writes on tmp_p and tmp_N
        const double t = ray_parallelogram_intersection(ray_origin, ray_direction, i, tmp_p, tmp_N);
        // We have intersection
        if (t >= 0)
        {
            // The point is before our current closest t
            if (t < closest_t)
            {
                closest_index = sphere_centers.size() + i;
                closest_t = t;
                p = tmp_p;
                N = tmp_N;
            }
        }
    }
    
    // TODO
    // Method (1): Traverse every triangle and return the closest hit.
    // Method (2): Traverse the BVH tree and test the intersection with a
    // triangles at the leaf nodes that intersects the input ray.
if (closest_index == -1){
        return false;
    } 
    return true;
}



////////////////////////////////////////////////////////////////////////////////
// Raytracer code
////////////////////////////////////////////////////////////////////////////////
//Checks if the light is visible
// code from a3
bool is_light_visible(const Vector3d &ray_origin, const Vector3d &ray_direction, const Vector3d &light_position)
{
    // TODO: Determine if the light is visible here
    //Use find_nearest_object
    Vector3d p;
    Vector3d N;
    long result = find_nearest_object(ray_origin, ray_direction, p, N);
    if (result != true){
        return true;
    }
    // removed for clarity
    // double x = (light_position-p).dot(ray_direction);
    // if(x > 0){
    //     return false;
    // }
    
    return false;
}

Vector4d shoot_ray(const Vector3d &ray_origin, const Vector3d &ray_direction, int max_bounce)
{
    //Intersection point and normal, these are output of find_nearest_object
    Vector3d p, N;

    const bool nearest_object = find_nearest_object(ray_origin, ray_direction, p, N);

    if (!nearest_object)
    {
        // Return a transparent color
        return Vector4d(0, 0, 0, 0);
    }

    // Ambient light contribution
    const Vector4d ambient_color = obj_ambient_color.array() * ambient_light.array();

    // Punctual lights contribution (direct lighting)
    Vector4d lights_color(0, 0, 0, 0);
    for (int i = 0; i < light_positions.size(); ++i)
    {
        const Vector3d &light_position = light_positions[i];
        const Vector4d &light_color = light_colors[i];

        Vector4d diff_color = obj_diffuse_color;

        // Diffuse contribution
        const Vector3d Li = (light_position - p).normalized();
        const Vector4d diffuse = diff_color * std::max(Li.dot(N), 0.0);
        // code from a3
         if(is_light_visible(p+1e-4*Li, Li, light_position)==false){
            continue;
        }

        // Specular contribution
        const Vector3d Hi = (Li - ray_direction).normalized();
        const Vector4d specular = obj_specular_color * std::pow(std::max(N.dot(Hi), 0.0), obj_specular_exponent);
        // Vector3d specular(0, 0, 0);

        // Attenuate lights according to the squared distance to the lights
        const Vector3d D = light_position - p;
        lights_color += (diffuse + specular).cwiseProduct(light_color) / D.squaredNorm();
    }
      
  
    Vector4d refl_color = obj_reflection_color;
    Vector4d reflection_color(0.1, 0.1, 0.1, 0.1);

    if(max_bounce <0 || max_bounce > 0){
    Vector3d v= (-1)*ray_direction.normalized();
    Vector3d reflection_direction =2 * N* (N.dot(v))-v;
    Vector4d colors = shoot_ray(p+1e-5*reflection_direction, reflection_direction, max_bounce);
    reflection_color = refl_color.cwiseProduct(colors);
    
    }
    // Rendering equation
    Vector4d C = ambient_color + lights_color+ reflection_color;

    //Set alpha to 1
    C(3) = 1;

    return C;
}

////////////////////////////////////////////////////////////////////////////////

void raytrace_scene()
{
    std::cout << "Simple ray tracer." << std::endl;

    int w = 640;
    int h = 480;
    MatrixXd R = MatrixXd::Zero(w, h);
    MatrixXd G = MatrixXd::Zero(w, h);
    MatrixXd B = MatrixXd::Zero(w, h);
    MatrixXd A = MatrixXd::Zero(w, h); // Store the alpha mask

    // The camera always points in the direction -z
    // The sensor grid is at a distance 'focal_length' from the camera center,
    // and covers an viewing angle given by 'field_of_view'.
    double aspect_ratio = double(w) / double(h);
    //TODO
    double image_y = tan(field_of_view/2)*focal_length; //TODO: compute the correct pixels size
    double image_x = tan(field_of_view/2)*focal_length * aspect_ratio;

    // The pixel grid through which we shoot rays is at a distance 'focal_length'
    const Vector3d image_origin(-image_x, image_y, camera_position[2] - focal_length);
    const Vector3d x_displacement(2.0 / w * image_x, 0, 0);
    const Vector3d y_displacement(0, -2.0 / h * image_y, 0);

    for (unsigned i = 0; i < w; ++i)
    {
        for (unsigned j = 0; j < h; ++j)
        {
            const Vector3d pixel_center = image_origin + (i + 0.5) * x_displacement + (j + 0.5) * y_displacement;

            // Prepare the ray
            Vector3d ray_origin;
            Vector3d ray_direction;

            if (is_perspective)
            {
                // Perspective camera
                ray_origin = camera_position;
                ray_direction = (pixel_center - camera_position).normalized();
            }
            else
            {
                // Orthographic camera
                ray_origin = pixel_center;
                ray_direction = Vector3d(0, 0, -1);
            }

            const Vector4d C = shoot_ray(ray_origin, ray_direction, max_bounce);
            R(i, j) = C(0);
            G(i, j) = C(1);
            B(i, j) = C(2);
            A(i, j) = C(3);
        }
    }

    // Save to png
    write_matrix_to_png(R, G, B, A, filename);
}

////////////////////////////////////////////////////////////////////////////////

int main(int argc, char *argv[])
{
    setup_scene();

    raytrace_scene();
    return 0;
}
