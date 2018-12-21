#include <lib/glm/glm/glm.hpp>
#include <graphics_framework.h>
#include <thread>
#include <iostream>


using namespace std;
using namespace graphics_framework;
using namespace glm;

// Applies the effect of a tropism defined in this function
vec3 apply_tropism(vec3 n, vec3 pos);

struct node
{
	vec3 pos;
	vec3 att_dir = vec3(0.0f); // Attraction direction
	vector<node*> children;


	node(vec3 pos)
	{
		this->pos = pos;
	}

	// Gets a node based on a number of iterations needed to reach it. !!Does not return the same element if the tree has been modified in any way!!
	node *get(int i)
	{
		int hits = 0;
		return get(i, hits);
	}

	// Returns the number of nodes in the tree
	int size()
	{
		int s = 0;
		return size(s);
	}

private:
	node *get(int i, int &hits)
	{
		if (i == hits)
			return this;
		hits++;
		for (node *n : children)
		{
			node *tmp = n->get(i, hits);
			if (tmp != nullptr)
				return tmp;
		}
		return nullptr;
	}

	int size(int &s)
	{
		for (node *n : children)
			n->size(s);
		return ++s;
	}

public:

	// Sets all attraction direction points to 0
	void clear_attractions()
	{
		att_dir = vec3(0.0f);
		for (node *n : children)
			n->clear_attractions();
	}

	// Returns the closest node in the tree
	node *closest_node(const vec3 &point)
	{
		node *closest = this;
		for (node *n : children)
		{
			node *closest_child = n->closest_node(point);
			if (length2(point - closest_child->pos) < length2(point - closest->pos))
				closest = closest_child;
		}
		return closest;
	}

	// Normalises all the attraction direction vectors
	void normalise_attractions()
	{
		if (att_dir != vec3(0.0f))
			att_dir = normalize(att_dir);
		for (node *n : children)
			n->normalise_attractions();
	}

	// Adds child nodes to nodes that have a non-zero att_dir along that vector at a distance d
	void colonise_nodes(const float &d)
	{
		for (node *n : children)
			n->colonise_nodes(d);
		vec3 zero = vec3(0.0f);
		if (att_dir != zero)
		{
			vec3 n = apply_tropism(this->att_dir, this->pos);
			vec3 branch = pos + (n * d);
			if (!(children.size() > 0 && children[children.size() - 1]->pos == branch))
				children.push_back(new node(branch));
		}
	}

	// Returns whether or not the node is closer to given point than distance d
	bool is_closer_than(const vec3 &point, const float &d)
	{
		if (length2(point - pos) < d * d)
			return true;
		for (node *n : children)
			if (n->is_closer_than(point, d))
				return true;
		return false;
	}

	// Returns all line segments in between nodes
	vector<pair<vec3, vec3>> get_segments()
	{
		vector<pair<vec3, vec3>> v;
		get_segments(v);
		return v;
	}

	void get_segments(vector<pair<vec3, vec3>> &s)
	{
		for (node *n : children)
		{
			s.push_back(pair<vec3, vec3>(pos, n->pos));
			n->get_segments(s);
		}
	}

	// Reduces the number of nodes by combining nodes with similar direction
	void reduce()
	{
		for (node *n : this->children)
			n->reduce();
		if (this->children.size() == 1)
			if (this->children[0]->children.size() == 1)
			{
				vec3 dir1 = normalize(this->children[0]->children[0]->pos - this->pos);
				vec3 dir2 = normalize(this->children[0]->pos - this->pos);
				if (dot(dir1, dir2) > 0.98f)
				{
					node *n = this->children[0];
					this->children[0] = this->children[0]->children[0];
					delete(n);
				}
			}
	}

	// Creates the tree body out of cylinders
	vector<mesh> create_body()
	{
		return create_body_helper().second;
	}

private:
	pair<float, vector<mesh>> create_body_helper()
	{
		vector<mesh> v;
		float scale = 0.03f;
		if (this->children.size() > 0)
			scale = 0.0f;
		float k = 2.3f;
		for (node *n : this->children)
		{
			pair<float, vector<mesh>> v1 = n->create_body_helper();
			v.insert(v.end(), v1.second.begin(), v1.second.end());

			v.push_back(mesh(geometry_builder().create_cylinder(1, 10)));
			float l = length(this->pos - n->pos);
			if (l != 0.0f)
			{
				vec3 up = vec3(normalize(n->pos - this->pos));
				vec3 forward;
				if (dot(up, vec3(1.0f, 0.0f, 0.0f)) < 1.0f)
					forward = vec3(normalize(cross(up, vec3(1.0f, 0.0f, 0.0f))));
				else
					forward = vec3(normalize(cross(up, vec3(0.0f, 0.0f, 1.0f))));
				v[v.size() - 1].get_transform().orientation = quatLookAt(forward, up);
				v[v.size() - 1].get_transform().scale = vec3(v1.first, l, v1.first);
			}
			else
				v[v.size() - 1].get_transform().scale = vec3(v1.first, v1.first, v1.first);
			v[v.size() - 1].get_transform().position = (this->pos + n->pos) / 2.0f;

			if (this->children.size() > 1)
				scale += powf(v1.first, k);
			else
				scale = v1.first;
		}
		if (this->children.size() > 1)
			scale = pow(scale, 1.0f / k);
		return pair<float, vector<mesh>>(scale, v);
	}
};


effect eff_red;
effect eff_green;
effect eff_blue;
effect eff_lambert;
effect eff_mask;
free_camera cam;

map<string, texture> masks;

// Cursor position
double cursor_x;
double cursor_y;

default_random_engine ran;

geometry screen_quad;
mesh plane;
frame_buffer f_buffer;


// Algorithm parameters
const uint32_t no_points = 1500; // Number of attraction points
const float dp = 0.1f; // Node placement distance
const float ri = dp * 20.0f;// * dp; // Radius of influence
const float dk = dp * 2.0f;// *dp; // Attraction point kill distance
bool finished = false;


vector<mesh> attraction_points;
vector<vec2> envelope_curve;
vector<vec3> points;
node *root;
vector<mesh> tree;
vector<pair<vec3, vec3>> segments;
vector<pair<vec3, vec3>> envelope_segments;
vector<mesh> envelope;

enum tropisms
{
	none,
	gravity,
	wind,
	attract,
	spin
};

tropisms tropism = none;

enum context
{
	start,
	define_crown,
	gen_tree
};

// Debug variables
bool use_debug = false;
vector<pair<vec3, vec3>> att_segments;
vector<mesh> attractions;
vector<pair<vec3, vec3>> next_branch_segments;
vector<mesh> next_branches;

bool next_frame = true;
bool no_wait = false;
context stage = start;
bool default_envelope = true;



bool initialise()
{
	// Set input mode - hide the cursor
	glfwSetInputMode(renderer::get_window(), GLFW_CURSOR, GLFW_CURSOR_DISABLED);
	// Capture initial mouse position
	glfwGetCursorPos(renderer::get_window(), &cursor_x, &cursor_y);
	return true;
}

// Use keyboard to move the camera - WSAD for xz and space, left control for y, mouse to rotate
void moveCamera(float delta_time)
{
	float speed = 0.1f;
	float mouse_sensitivity = 3.0;

	vec3 fw = cam.get_forward();
	fw.y = 0.0f;
	fw = normalize(fw);
	vec3 left = vec3(rotate(mat4(1), half_pi<float>(), vec3(0.0f, 1.0f, 0.0f)) * vec4(fw, 1.0));

	if (glfwGetKey(renderer::get_window(), GLFW_KEY_W))
		cam.set_position(cam.get_position() + fw * speed);
	if (glfwGetKey(renderer::get_window(), GLFW_KEY_S))
		cam.set_position(cam.get_position() + fw * -speed);
	if (glfwGetKey(renderer::get_window(), GLFW_KEY_A))
		cam.set_position(cam.get_position() + left * speed);
	if (glfwGetKey(renderer::get_window(), GLFW_KEY_D))
		cam.set_position(cam.get_position() + left * -speed);
	if (glfwGetKey(renderer::get_window(), GLFW_KEY_SPACE))
		cam.set_position(cam.get_position() + vec3(0.0f, 1.0f, 0.0f) * speed);
	if (glfwGetKey(renderer::get_window(), GLFW_KEY_LEFT_CONTROL))
		cam.set_position(cam.get_position() + vec3(0.0f, 1.0f, 0.0f) * -speed);

	static double ratio_width = quarter_pi<float>() / static_cast<float>(renderer::get_screen_width());
	static double ratio_height = (quarter_pi<float>() * (1.0f / static_cast<float>(renderer::get_screen_width())));

	double prev_x = cursor_x;
	double prev_y = cursor_y;

	glfwGetCursorPos(renderer::get_window(), &cursor_x, &cursor_y);

	double delta_x = cursor_x - prev_x;
	double delta_y = cursor_y - prev_y;

	float theta_y = delta_x * ratio_width * mouse_sensitivity;
	float theta_x = delta_y * ratio_height * mouse_sensitivity;

	cam.rotate(theta_y, -theta_x);
}

// Calculates PV part of the MVP matrix depending on the camera curently selected
mat4 calculatePV()
{
	return cam.get_projection() * cam.get_view();
}

// Makes a 2d curve which can later be used to determine if a point is inside the envelope or not. !!Curve should be laid out top to bottom in descending height; all values must be positive!!
vector<vec2> default_envelope_curve()
{
	vector<vec2> curve;
	curve.push_back(vec2(0.0f, 8.0f));
	curve.push_back(vec2(0.6f, 7.8f));
	curve.push_back(vec2(1.2f, 7.4f));
	curve.push_back(vec2(1.8f, 6.9f));
	curve.push_back(vec2(2.2f, 6.2f));
	curve.push_back(vec2(2.5f, 5.3f));
	curve.push_back(vec2(2.4f, 4.5f));
	curve.push_back(vec2(2.0f, 4.0f));
	curve.push_back(vec2(1.7f, 3.5f));
	curve.push_back(vec2(1.3f, 3.0f));
	curve.push_back(vec2(0.9f, 2.5f));
	return curve;
}

// Makes a segment vector from envelope curve
vector<pair<vec3, vec3>> curve_to_segments(const vector<vec2> &v)
{
	int count = v.size();
	vector<pair<vec3, vec3>> seg;
	if (count < 1)
		return seg;
	seg.push_back(pair<vec3, vec3>(vec3(0.0f, v[0].y, 0.0f), vec3(v[0].x, v[0].y, 0.0f)));
	if (count == 1)
		return seg;
	for (int i = 0; i < count - 1; i++)
	{
		seg.push_back(pair<vec3, vec3>(vec3(v[i].x, v[i].y, 0.0f), vec3(v[i + 1].x, v[i + 1].y, 0.0f)));
	}

	seg.push_back(pair<vec3, vec3>(vec3(0.0f, v[count - 1].y, 0.0f), vec3(v[count - 1].x, v[count - 1].y, 0.0f)));
	seg.push_back(pair<vec3, vec3>(seg[0].first, seg[seg.size() - 1].first));
	return seg;
}

// Returns true if a point is inside a rotated curve envelope. The curve is rotated around the y axis
bool inside_rotated_curve(const vec3 &point, const vector<vec2> &curve)
{
	// Check if above below curve
	if (point.y > curve[0].y)
		return false;
	if (point.y < curve[curve.size() - 1].y)
		return false;

	for (int i = 0; i < curve.size() - 1; i++)
		if (point.y >= curve[i + 1].y)
		{
			float d2 = (point.x * point.x) + (point.z * point.z);
			float alpha = (curve[i].y - point.y) / (curve[i].y - curve[i + 1].y);
			float cd = (curve[i + 1].x - curve[i].x) * alpha + curve[i].x;
			if (d2 > cd * cd)
				return false;
			else
				return true;
		}
	return false;
}

// Populates the envelope with a uniform distribution of attraction points
vector<vec3> populate_envelope(const vector<vec2> &curve)
{
	float maxx = 0.0f;
	float maxy = 0.0f;
	float miny = 100.0f;
	for (vec2 p : curve)
	{
		if (maxx < p.x)
			maxx = p.x;
		if (maxy < p.y)
			maxy = p.y;
		if (miny > p.y)
			miny = p.y;
	}
	uniform_real_distribution<float> dist_xz(-maxx, maxx);
	uniform_real_distribution<float> dist_y(miny, maxy);

	vector<vec3> points;
	vec3 point;
	for (int i = 0; i < no_points; i++)
	{
		point = vec3(dist_xz(ran), dist_y(ran), dist_xz(ran));
		if (inside_rotated_curve(point, curve))
			points.push_back(point);
		else
		{
			i--;
			continue;
		}
	}
	return points;
}

// Creates a structure representing the tree. The structure is stored in the provided root
void create_node_tree(vector<vec3> points, node *root)
{
	while (points.size() > 0)
	{
		int size = root->size();
		root->clear_attractions();
		// Adds attraction vectors to the tree
		for (const vec3 &p : points)
		{
			node *closest = root->closest_node(p);
			// Only add attraction if point is within the radius of influence
			if (length2(p - closest->pos) <= ri * ri)
				closest->att_dir += normalize(p - closest->pos);
		}
		root->normalise_attractions();
		root->colonise_nodes(dp);
		// If no nodes are added add one above the last node
		if (size == root->size())
			if (root->get(size - 1)->pos.y > 8.0f)
				return;
			else
				root->get(size - 1)->children.push_back(new node(root->get(size - 1)->pos + vec3(0.0f, dp, 0.0f)));
		// Purge attraction points that are within kill distance
		for (int i = 0; i < points.size(); i++)
			if (root->is_closer_than(points[i], dk))
			{
				points.erase(points.begin() + i);
				i--;
			}
	}
}

// Applies the effect of a tropism defined in this function
vec3 apply_tropism(vec3 n, vec3 pos)
{
	switch (tropism)
	{
	case none:
		return n;
		break;
	case gravity:
		return normalize(n + vec3(0.0f, -0.6f, 0.0f));
		break;
	case attract:
		if (pos.x + pos.z != 0)
		{
			vec3 core = vec3(0.0f, pos.y, 0.0f);
			return normalize(normalize(core - pos) * 1.0f + n);
		}
		return n;
		break;
	case spin:
		if (pos.x + pos.z != 0)
		{
			vec3 core = vec3(0.0f, pos.y, 0.0f);
			vec3 perp = normalize(cross(vec3(0.0f, 1.0f, 0.0f), pos - core));
			return normalize((perp * 1.0f) * dot(n, pos - core) + n);
		}
		return n;
		break;
	default:
		return n;
		break;
	}
}

// Does a single iteration of the algorithm
void create_tree_single_pass(vector<vec3> &points, node *root)
{
	if (points.size() == 0)
		return;
	int size = root->size();
	root->clear_attractions();
	// Adds attraction vectors to the tree
	for (const vec3 &p : points)
	{
		node *closest = root->closest_node(p);
		// Only add attraction if point is within the radius of influence
		if (length2(p - closest->pos) <= ri * ri)
		{
			closest->att_dir += normalize(p - closest->pos);
			if (use_debug)
				att_segments.push_back(pair<vec3, vec3>(p, closest->pos));
		}
	}
	root->normalise_attractions();
	if (use_debug)
		for (int i = 0; i < root->size(); i++)
			if (root->get(i)->att_dir != vec3(0.0))
				next_branch_segments.push_back(pair<vec3, vec3>(root->get(i)->pos, root->get(i)->pos + root->get(i)->att_dir * dp));
	root->colonise_nodes(dp);
	// If no nodes are added add one above the last node
	static bool found_points_yet = false;
	if (size == root->size())
		if (root->get(size - 1)->pos.y > envelope_curve[0].y)
			finished = true;
		else
			if (!found_points_yet)
				root->get(size - 1)->children.push_back(new node(root->get(size - 1)->pos + vec3(0.0f, dp, 0.0f)));
			else
				finished = true;
	else
		found_points_yet = true;
	// Purge attraction points that are within kill distance
	for (int i = 0; i < points.size(); i++)
		if (root->is_closer_than(points[i], dk))
		{
			points.erase(points.begin() + i);
			i--;
		}
}

// Creates cylinder meshes for given segments
void create_meshes(const vector<pair<vec3, vec3>> &seg, vector<mesh> &v)
{
	for (pair<vec3, vec3> p : seg)
	{
		bool exists = false;
		for (mesh &m : v)
			if (m.get_transform().position == (p.first + p.second) / 2.0f)
				exists = true;
		if (exists)
			continue;
		v.push_back(mesh(geometry_builder().create_box()));
		float l = length(p.first - p.second);
		if (l != 0.0f)
		{
			vec3 up = vec3(normalize(p.second - p.first));
			vec3 forward;
			if (dot(up, vec3(1.0f, 0.0f, 0.0f)) < 1.0f)
				forward = vec3(normalize(cross(up, vec3(1.0f, 0.0f, 0.0f))));
			else
				forward = vec3(normalize(cross(up, vec3(0.0f, 0.0f, 1.0f))));
			v[v.size() - 1].get_transform().orientation = quatLookAt(forward, up);
			v[v.size() - 1].get_transform().scale = vec3(0.03f, l, 0.03f);
		}
		else
			v[v.size() - 1].get_transform().scale = vec3(0.03f, 0.03f, 0.03f);
		v[v.size() - 1].get_transform().position = (p.first + p.second) / 2.0f;
	}
}

// Uses default envelope to generate attraction points
void prep_for_generating()
{
	points = populate_envelope(envelope_curve);
	for (const vec3 &v : points)
	{
		//attraction_points.push_back(mesh(geometry(geometry_builder().create_sphere(10, 10, vec3(ri)))));
		attraction_points.push_back(mesh(geometry(geometry_builder().create_box(vec3(0.05f)))));
		attraction_points[attraction_points.size() - 1].get_transform().position = vec3(v);
	}
	// create root for the tree
	root = new node(vec3(0.0f));
}

// Handles the controls except for camera movement
void controls(const float &dt)
{
	static float cd = 0.0f;
	cd -= dt;

	switch (stage)
	{
	case start:
	{
		// Default envelope
		if (glfwGetKey(renderer::get_window(), GLFW_KEY_1) && cd <= 0.0f)
		{
			default_envelope = true;
			prep_for_generating();
			stage = gen_tree;
			cd = 0.2f;
		}
		// Custom envelope
		else if (glfwGetKey(renderer::get_window(), GLFW_KEY_2) && cd <= 0.0f)
		{
			default_envelope = false;
			stage = define_crown;

			// Handle envelope drawing stuff
			envelope_curve.clear();
			envelope_segments.clear();
			envelope.clear();
			envelope_curve.push_back(vec2(0.2f, 8.0f));
			envelope_segments = curve_to_segments(envelope_curve);
			create_meshes(envelope_segments, envelope);

			cd = 0.2f;
		}
	}
		break;
	case define_crown:
	{
		// Enter to add point to curve
		if (glfwGetKey(renderer::get_window(), GLFW_KEY_ENTER) && cd <= 0.0f)
		{
			envelope_curve.push_back(envelope_curve[envelope_curve.size() - 1] + vec2(0.5f, -0.5f));
			// Handle envelope drawing stuff
			envelope_segments.clear();
			envelope.clear();
			envelope_segments = curve_to_segments(envelope_curve);
			create_meshes(envelope_segments, envelope);
			cd = 0.2f;
		}
		// 1 to finish making curve
		if (glfwGetKey(renderer::get_window(), GLFW_KEY_1) && cd <= 0.0f)
		{
			if (envelope_curve.size() < 2)
				break;
			prep_for_generating();
			stage = gen_tree;
			cd = 0.2f;
		}
		// Arrow keys to move point
		{
			if (glfwGetKey(renderer::get_window(), GLFW_KEY_LEFT))
			{
				if (envelope_curve[envelope_curve.size() - 1].x <= 0.0)
					break;
				envelope_curve[envelope_curve.size() - 1] -= vec2(1.0f * dt, 0.0f);
				// Handle envelope drawing stuff
				envelope_segments.clear();
				envelope.clear();
				envelope_segments = curve_to_segments(envelope_curve);
				create_meshes(envelope_segments, envelope);
			}
			if (glfwGetKey(renderer::get_window(), GLFW_KEY_RIGHT))
			{
				envelope_curve[envelope_curve.size() - 1] += vec2(1.0f * dt, 0.0f);
				// Handle envelope drawing stuff
				envelope_segments.clear();
				envelope.clear();
				envelope_segments = curve_to_segments(envelope_curve);
				create_meshes(envelope_segments, envelope);
			}
			if (glfwGetKey(renderer::get_window(), GLFW_KEY_UP))
			{
				if (envelope_curve.size() > 1 && envelope_curve[envelope_curve.size() - 1].y >= envelope_curve[envelope_curve.size() - 2].y)
					break;
				envelope_curve[envelope_curve.size() - 1] += vec2(0.0f, 1.0f * dt);
				// Handle envelope drawing stuff
				envelope_segments.clear();
				envelope.clear();
				envelope_segments = curve_to_segments(envelope_curve);
				create_meshes(envelope_segments, envelope);
			}
			if (glfwGetKey(renderer::get_window(), GLFW_KEY_DOWN))
			{
				envelope_curve[envelope_curve.size() - 1] -= vec2(0.0f, 1.0f * dt);
				// Handle envelope drawing stuff
				envelope_segments.clear();
				envelope.clear();
				envelope_segments = curve_to_segments(envelope_curve);
				create_meshes(envelope_segments, envelope);
			}
		}
	}
		break;
	case gen_tree:
		if (glfwGetKey(renderer::get_window(), GLFW_KEY_1) && cd <= 0.0f)
		{
			next_frame = true;
			cd = 0.2f;
		}

		if (glfwGetKey(renderer::get_window(), GLFW_KEY_ENTER) && cd <= 0.0f)
		{
			no_wait = !no_wait;
			cd = 0.2f;
		}

		if (glfwGetKey(renderer::get_window(), GLFW_KEY_F1) && cd <= 0.0f)
		{
			use_debug = !use_debug;
			cd = 0.2f;
		}

		if (glfwGetKey(renderer::get_window(), GLFW_KEY_DELETE) && cd <= 0.0f)
		{
			cout << root->size() << endl;
			root->reduce();
			segments.clear();
			tree.clear();
			segments = root->get_segments();
			create_meshes(segments, tree);
			cout << root->size() << endl;

			cd = 0.2f;
		}

		if (glfwGetKey(renderer::get_window(), GLFW_KEY_HOME) && cd <= 0.0f)
		{
			tree = root->create_body();

			cd = 0.2f;
		}

		break;
	}
}

bool load_content()
{
	// Set here to show default when choosing default or custom
	envelope_curve = default_envelope_curve();
	envelope_segments = curve_to_segments(envelope_curve);
	create_meshes(envelope_segments, envelope);
	// Screen quad
	{
		vector<vec3> positions{ vec3(-1.0f, -1.0f, 0.0f), vec3(1.0f, -1.0f, 0.0f), vec3(-1.0f, 1.0f, 0.0f),	vec3(1.0f, 1.0f, 0.0f) };
		vector<vec2> tex_coords{ vec2(0.0, 0.0), vec2(1.0f, 0.0f), vec2(0.0f, 1.0f), vec2(1.0f, 1.0f) };
		screen_quad.set_type(GL_TRIANGLE_STRIP);
		screen_quad.add_buffer(positions, BUFFER_INDEXES::POSITION_BUFFER);
		screen_quad.add_buffer(tex_coords, BUFFER_INDEXES::TEXTURE_COORDS_0);
	}

	f_buffer = frame_buffer(renderer::get_screen_width(), renderer::get_screen_height());

	plane = mesh(geometry(geometry_builder().create_plane(5, 5, false)));

	// Build effects
	{
		// Load in shaders
		eff_red.add_shader("res/shaders/core.vert", GL_VERTEX_SHADER);
		eff_red.add_shader("res/shaders/red.frag", GL_FRAGMENT_SHADER);
		// Build effect
		eff_red.build();

		// Load in shaders
		eff_green.add_shader("res/shaders/core.vert", GL_VERTEX_SHADER);
		eff_green.add_shader("res/shaders/green.frag", GL_FRAGMENT_SHADER);
		// Build effect
		eff_green.build();

		// Load in shaders
		eff_blue.add_shader("res/shaders/core.vert", GL_VERTEX_SHADER);
		eff_blue.add_shader("res/shaders/blue.frag", GL_FRAGMENT_SHADER);
		// Build effect
		eff_blue.build();

		eff_lambert.add_shader("res/shaders/lambert.vert", GL_VERTEX_SHADER);
		eff_lambert.add_shader("res/shaders/lambert.frag", GL_FRAGMENT_SHADER);
		// Build effect
		eff_lambert.build();

		eff_mask.add_shader("res/shaders/minimal_quad.vert", GL_VERTEX_SHADER);
		eff_mask.add_shader("res/shaders/mask.frag", GL_FRAGMENT_SHADER);
		// Build effect
		eff_mask.build();
	}

	masks["choose_envelope"] = texture("res/textures/choose_crown.png", true, true);
	masks["define_envelope"] = texture("res/textures/define_crown.png", true, true);
	masks["gen"] = texture("res/textures/tree.png", true, true);

	// Set camera properties
	cam.set_position(vec3(0.0f, 3.0f, 10.0f));
	//cam.set_target(vec3(0.0f, 3.0f, 0.0f));
	cam.set_projection(quarter_pi<float>(), renderer::get_screen_aspect(), 0.1f, 1000.0f);
	return true;
}


bool update(float delta_time)
{
	controls(delta_time);

	switch (stage)
	{
	case start:
		break;
	case define_crown:
		break;
	case gen_tree:
		if ((!finished && next_frame) || no_wait)
		{
			if (tropism == wind)
			{
				for (vec3 &p : points)
				{
					p += vec3(0.04f, 0.0f, 0.0f);
				}
			}
			next_frame = false;
			att_segments.clear();
			attractions.clear();
			next_branch_segments.clear();
			next_branches.clear();
			//create_tree_single_pass(points, root);
			// Clear meshes that no longer represent attraction points
			for (int i = 0; i < attraction_points.size(); i++)
			{
				bool exists = false;
				for (vec3 p : points)
				{
					if (p == attraction_points[i].get_transform().position)
						exists = true;
				}
				if (exists == false)
				{
					attraction_points.erase(attraction_points.begin() + i);
					i--;
				}
			}
			segments = root->get_segments();
			create_meshes(segments, tree);
			create_tree_single_pass(points, root);
			if (use_debug)
			{
				create_meshes(att_segments, attractions);
				create_meshes(next_branch_segments, next_branches);
			}
		}
		break;
	default:
		break;
	}

	// Camera stuff
	moveCamera(delta_time);
	cam.update(delta_time);
	return true;
}

bool render()
{
	mat4 MVP;
	mat4 PV = calculatePV();

	switch (stage)
	{
	case start:
	{
		renderer::set_render_target(f_buffer);
		renderer::clear();

		// Render ground plane
		renderer::bind(eff_lambert);
		MVP = PV * plane.get_transform().get_transform_matrix();
		glUniformMatrix4fv(eff_lambert.get_uniform_location("MVP"), 1, GL_FALSE, value_ptr(MVP));
		glUniformMatrix3fv(eff_lambert.get_uniform_location("NM"), 1, GL_FALSE, value_ptr(plane.get_transform().get_normal_matrix()));
		renderer::render(plane);

		// Render default envelope
		renderer::bind(eff_blue);
		for (mesh m : envelope)
		{
			mat4 MVP = PV * m.get_transform().get_transform_matrix();
			// Set MVP matrix uniform
			glUniformMatrix4fv(eff_blue.get_uniform_location("MVP"), 1, GL_FALSE, value_ptr(MVP));
			renderer::render(m);
		}

		// Render frame to screen with menu mask
		renderer::set_render_target();
		renderer::clear();
		renderer::bind(eff_mask);
		renderer::bind(f_buffer.get_frame(), 0);
		glUniform1i(eff_mask.get_uniform_location("tex"), 0);
		renderer::bind(masks["choose_envelope"], 1);
		glUniform1i(eff_mask.get_uniform_location("alpha_map"), 1);
		renderer::render(screen_quad);
	}
	break;

	case define_crown:
	{
		renderer::set_render_target(f_buffer);
		renderer::clear();

		// Render ground plane
		renderer::bind(eff_lambert);
		MVP = PV * plane.get_transform().get_transform_matrix();
		glUniformMatrix4fv(eff_lambert.get_uniform_location("MVP"), 1, GL_FALSE, value_ptr(MVP));
		glUniformMatrix3fv(eff_lambert.get_uniform_location("NM"), 1, GL_FALSE, value_ptr(plane.get_transform().get_normal_matrix()));
		renderer::render(plane);

		// Render envelope
		renderer::bind(eff_blue);
		for (mesh m : envelope)
		{
			mat4 MVP = PV * m.get_transform().get_transform_matrix();
			// Set MVP matrix uniform
			glUniformMatrix4fv(eff_blue.get_uniform_location("MVP"), 1, GL_FALSE, value_ptr(MVP));
			renderer::render(m);
		}

		// Render frame to screen with menu mask
		renderer::set_render_target();
		renderer::clear();
		renderer::bind(eff_mask);
		renderer::bind(f_buffer.get_frame(), 0);
		glUniform1i(eff_mask.get_uniform_location("tex"), 0);
		renderer::bind(masks["define_envelope"], 1);
		glUniform1i(eff_mask.get_uniform_location("alpha_map"), 1);
		renderer::render(screen_quad);
	}
	break;

	case gen_tree:
	{
		// Render tree
		renderer::set_render_target(f_buffer);
		renderer::clear();
		renderer::bind(eff_lambert);

		MVP = PV * plane.get_transform().get_transform_matrix();
		glUniformMatrix4fv(eff_lambert.get_uniform_location("MVP"), 1, GL_FALSE, value_ptr(MVP));
		glUniformMatrix3fv(eff_lambert.get_uniform_location("NM"), 1, GL_FALSE, value_ptr(plane.get_transform().get_normal_matrix()));
		renderer::render(plane);

		glUniform3fv(eff_lambert.get_uniform_location("eyePosition"), 1, value_ptr(cam.get_position()));
		for (mesh m : tree)
		{
			MVP = PV * m.get_transform().get_transform_matrix();
			// Set MVP matrix uniform
			glUniformMatrix4fv(eff_lambert.get_uniform_location("MVP"), 1, GL_FALSE, value_ptr(MVP));
			glUniformMatrix3fv(eff_lambert.get_uniform_location("NM"), 1, GL_FALSE, value_ptr(m.get_transform().get_normal_matrix()));
			renderer::render(m);
		}

		// Render Attraction points, vectors and next branch position
		if (use_debug)
		{
			renderer::bind(eff_green);
			for (mesh m : attraction_points)
			{
				mat4 MVP = PV * m.get_transform().get_transform_matrix();
				// Set MVP matrix uniform
				glUniformMatrix4fv(eff_green.get_uniform_location("MVP"), 1, GL_FALSE, value_ptr(MVP));
				renderer::render(m);
			}

			renderer::bind(eff_blue);
			for (mesh m : attractions)
			{
				mat4 MVP = PV * m.get_transform().get_transform_matrix();
				// Set MVP matrix uniform
				glUniformMatrix4fv(eff_blue.get_uniform_location("MVP"), 1, GL_FALSE, value_ptr(MVP));
				renderer::render(m);
			}

			renderer::bind(eff_red);
			for (mesh m : next_branches)
			{
				mat4 MVP = PV * m.get_transform().get_transform_matrix();
				// Set MVP matrix uniform
				glUniformMatrix4fv(eff_red.get_uniform_location("MVP"), 1, GL_FALSE, value_ptr(MVP));
				renderer::render(m);
			}
		}

		// Render frame to screen with menu mask
		renderer::set_render_target();
		renderer::clear();
		renderer::bind(eff_mask);
		renderer::bind(f_buffer.get_frame(), 0);
		glUniform1i(eff_mask.get_uniform_location("tex"), 0);
		renderer::bind(masks["gen"], 1);
		glUniform1i(eff_mask.get_uniform_location("alpha_map"), 1);
		renderer::render(screen_quad);
	}
	break;
	default:
		break;
	}


	return true;
}

void main() {
	// Create application
	app application("Graphics Coursework");
	// Set load content, update and render methods
	application.set_initialise(initialise);
	application.set_load_content(load_content);
	application.set_update(update);
	application.set_render(render);
	// Run application
	application.run();
}