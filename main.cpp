#include <lib/glm/glm/glm.hpp>
#include <graphics_framework.h>
#include <thread>



using namespace std;
using namespace graphics_framework;
using namespace glm;

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
			if (length2(point - closest_child->pos) < length2(point - pos))
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
			if (!(children.size() > 0 && children[children.size() - 1]->pos == pos + (att_dir * d)))
				children.push_back(new node(pos + (att_dir * d)));
	}

	// Returns whether or not the node is closer to given point thand istance d
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
};


effect eff;
effect eff_lambert;
free_camera cam;

// Cursor position
double cursor_x;
double cursor_y;

default_random_engine ran;

geometry screen_quad;


// Algorithm parameters
const uint32_t no_points = 10; // Number of attraction points
const float dp = 0.2f; // Node placement distance
const float ri = 15.0f * dp; // Radius of influence
const float dk = 3.0f * dp; // Attraction point kill distance


vector<mesh> other_meshes;
vector<mesh> attraction_points;
vector<vec2> envelope_curve;
vector<vec3> points;
node *root;
vector<mesh> tree;
vector<pair<vec3, vec3>> segments;

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
	float speed = 0.3f;
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
vector<vec2> rotated_curve_envelope()
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
			float d = (point.x * point.x) + (point.z * point.z);
			float alpha = (point.y - curve[i + 1].y) / (curve[i].y - curve[i + 1].y);
			float cd = (curve[i + 1].x - curve[i].x) * alpha + curve[i].x;
			if (d > cd * cd)
				return false;
			else
				return true;
		}
	return false;
}

// Populates the envelope with a uniform distribution of attraction points
vector<vec3> populate_envelope(const vector<vec2> &curve)
{
	// Hardcoded values !!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!
	uniform_real_distribution<float> dist_xz(-2.5f, 2.5f);
	uniform_real_distribution<float> dist_y(2.5f, 8.0f);

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

//
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
			closest->att_dir += normalize(p - closest->pos);
	}
	root->normalise_attractions();
	root->colonise_nodes(dp);
	// If no nodes are added add one above the last node
	if (size == root->size())
		if (root->get(size - 1)->pos.y > 8.0f)
			points.erase(points.begin(), points.end());
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
		vec3 up = vec3(normalize(p.second - p.first));
		vec3 forward = vec3(normalize(cross(up, vec3(1.0f, 0.0f, 0.0f))));
		v[v.size() - 1].get_transform().orientation = quatLookAt(forward, up);
		v[v.size() - 1].get_transform().scale = vec3(0.05f, length(p.first - p.second), 0.05f);
		v[v.size() - 1].get_transform().position = (p.first + p.second) / 2.0f;
	}
}

bool load_content()
{
	// Screen quad
	{
		vector<vec3> positions{ vec3(-1.0f, -1.0f, 0.0f), vec3(1.0f, -1.0f, 0.0f), vec3(-1.0f, 1.0f, 0.0f),	vec3(1.0f, 1.0f, 0.0f) };
		vector<vec2> tex_coords{ vec2(0.0, 0.0), vec2(1.0f, 0.0f), vec2(0.0f, 1.0f), vec2(1.0f, 1.0f) };
		screen_quad.set_type(GL_TRIANGLE_STRIP);
		screen_quad.add_buffer(positions, BUFFER_INDEXES::POSITION_BUFFER);
		screen_quad.add_buffer(tex_coords, BUFFER_INDEXES::TEXTURE_COORDS_0);
	}


	// Testing setup
	envelope_curve = rotated_curve_envelope();
	/*for (const vec2 &v : envelope_curve)
	{
		other_meshes.push_back(mesh(geometry(geometry_builder().create_box(vec3(0.1f)))));
		other_meshes[other_meshes.size() - 1].get_transform().position = vec3(v, 0.0f);
	}*/
	points = populate_envelope(envelope_curve);
	for (const vec3 &v : points)
	{
		attraction_points.push_back(mesh(geometry(geometry_builder().create_box(vec3(0.05f)))));
		attraction_points[attraction_points.size() - 1].get_transform().position = vec3(v);
	}

	root = new node(vec3(0.0f));
	//create_node_tree(points, root);
	//segments = root->get_segments();

	//tree = create_meshes(segments);

	//for (int i = 0; i < root->size(); i++)
	//{
	//	tree.push_back(mesh(geometry(geometry_builder().create_box(vec3(0.05f)))));
	//	tree[i].get_transform().position = root->get(i)->pos;
	//}

	

	// Load in shaders
	eff.add_shader("res/shaders/core.vert", GL_VERTEX_SHADER);
	eff.add_shader("res/shaders/green.frag", GL_FRAGMENT_SHADER);
	// Build effect
	eff.build();

	eff_lambert.add_shader("res/shaders/lambert.vert", GL_VERTEX_SHADER);
	eff_lambert.add_shader("res/shaders/lambert.frag", GL_FRAGMENT_SHADER);
	// Build effect
	eff_lambert.build();

	// Set camera properties
	cam.set_position(vec3(0.0f, 3.0f, 10.0f));
	//cam.set_target(vec3(0.0f, 3.0f, 0.0f));
	cam.set_projection(quarter_pi<float>(), renderer::get_screen_aspect(), 0.1f, 1000.0f);
	return true;
}


bool update(float delta_time)
{
	create_tree_single_pass(points, root);
	segments = root->get_segments();
	create_meshes(segments, tree);

	// Camera stuff
	moveCamera(delta_time);
	cam.update(delta_time);
	return true;
}

bool render()
{
	mat4 PV = calculatePV();
	// Bind effect
	renderer::bind(eff);

	//for (mesh m : other_meshes)
	//{
	//	mat4 MVP = PV * m.get_transform().get_transform_matrix();
	//	// Set MVP matrix uniform
	//	glUniformMatrix4fv(eff.get_uniform_location("MVP"), 1, GL_FALSE, value_ptr(MVP));
	//	renderer::render(m);
	//}

	renderer::bind(eff_lambert);
	for (mesh m : tree)
	{
		mat4 MVP = PV * m.get_transform().get_transform_matrix();
		// Set MVP matrix uniform
		glUniformMatrix4fv(eff_lambert.get_uniform_location("MVP"), 1, GL_FALSE, value_ptr(MVP));
		glUniformMatrix3fv(eff_lambert.get_uniform_location("NM"), 1, GL_FALSE, value_ptr(m.get_transform().get_normal_matrix()));
		glUniform3fv(eff_lambert.get_uniform_location("eyePosition"), 1, value_ptr(cam.get_position()));
		renderer::render(m);
	}

	renderer::bind(eff);
	for (mesh m : attraction_points)
	{
		mat4 MVP = PV * m.get_transform().get_transform_matrix();
		// Set MVP matrix uniform
		glUniformMatrix4fv(eff.get_uniform_location("MVP"), 1, GL_FALSE, value_ptr(MVP));
		renderer::render(m);
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