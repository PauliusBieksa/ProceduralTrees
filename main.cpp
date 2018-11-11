#include <lib/glm/glm/glm.hpp>
#include <graphics_framework.h>



using namespace std;
using namespace graphics_framework;
using namespace glm;


effect eff;
effect eff_red;
free_camera cam;

// Cursor position
double cursor_x;
double cursor_y;

default_random_engine ran;

geometry screen_quad;

vector<mesh> other_meshes;
vector<mesh> nodes;
vector<vec2> envelope_curve;
const uint32_t no_points = 400; // Number of attraction points

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

// Calculates PV part of the MVP matrix depending on the camera curently selected
mat4 calculatePV()
{
	return cam.get_projection() * cam.get_view();
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
	for (const vec2 &v : envelope_curve)
	{
		other_meshes.push_back(mesh(geometry(geometry_builder().create_box(vec3(0.05f)))));
		other_meshes[other_meshes.size() - 1].get_transform().position = vec3(v, 0.0f);
	}
	vector<vec3> points = populate_envelope(envelope_curve);
	for (const vec3 &v : points)
	{
		nodes.push_back(mesh(geometry(geometry_builder().create_box(vec3(0.05f)))));
		nodes[nodes.size() - 1].get_transform().position = vec3(v);
	}

	// Load in shaders
	eff.add_shader("res/shaders/core.vert", GL_VERTEX_SHADER);
	eff.add_shader("res/shaders/green.frag", GL_FRAGMENT_SHADER);
	// Build effect
	eff.build();

	eff_red.add_shader("res/shaders/core.vert", GL_VERTEX_SHADER);
	eff_red.add_shader("res/shaders/red.frag", GL_FRAGMENT_SHADER);
	// Build effect
	eff_red.build();

	// Set camera properties
	cam.set_position(vec3(0.0f, 3.0f, 10.0f));
	//cam.set_target(vec3(0.0f, 3.0f, 0.0f));
	cam.set_projection(quarter_pi<float>(), renderer::get_screen_aspect(), 0.1f, 1000.0f);
	return true;
}


bool update(float delta_time)
{
	moveCamera(delta_time);
	// Update the camera
	cam.update(delta_time);
	return true;
}

bool render()
{
	// Bind effect
	renderer::bind(eff);

	// Render geometry
	for (mesh m : other_meshes)
	{
		mat4 MVP = calculatePV() * m.get_transform().get_transform_matrix();
		// Set MVP matrix uniform
		glUniformMatrix4fv(eff.get_uniform_location("MVP"), 1, GL_FALSE, value_ptr(MVP));
		renderer::render(m);
	}

	renderer::bind(eff_red);
	// Render geometry
	for (mesh m : nodes)
	{
		mat4 MVP = calculatePV() * m.get_transform().get_transform_matrix();
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