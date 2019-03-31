#include "ofApp.h"
#include "ofxRaccoonImGui.hpp"
#include <stack>
#include "peseudo_random.hpp"

std::vector<float> weights = { 1.0 / 2.0, 1.0 / 3.0, 1.0 / 12.0, 1.0 / 12.0 };

class AliasMethod {
public:
	void prepare(const std::vector<float> &weights) {
		probs.clear();
		buckets.clear();

		float w_sum = std::accumulate(weights.begin(), weights.end(), 0.0f, [](float a, float b) { return a + b; });

		int N = weights.size();
		probs.resize(N);
		buckets.resize(N);
		for (int i = 0; i < N; ++i) {
			probs[i] = weights[i] / w_sum;
			buckets[i].height = probs[i] * N;
		}

		//float h_sum = 0.0f;
		//for (int i = 0; i < N; ++i) {
		//	h_sum += buckets[i].height;
		//}
		//float h_avg = h_sum / N;

		std::stack<int> lower;
		std::stack<int> upper;

		for (int i = 0; i < N; ++i) {
			if (buckets[i].height < 1.0f) {
				lower.push(i);
			}
			else {
				upper.push(i);
			}
		}

		for (;;) {
			if (lower.empty() || upper.empty()) {
				break;
			}

			int lower_index = lower.top();
			lower.pop();

			int upper_index = upper.top();
			upper.pop();

			assert(1.0f <= buckets[upper_index].height);

			float mov = 1.0f - buckets[lower_index].height;
			buckets[upper_index].height -= mov;
			buckets[lower_index].alias = upper_index;

			if (buckets[upper_index].height < 1.0f) {
				lower.push(upper_index);
			}
			else {
				upper.push(upper_index);
			}

			// lower is already completed
		}
	}

	float probability(int i) const {
		return probs[i];
	}
	int sample(float u0, float u1) const {
		float indexf = u0 * buckets.size();
		int index = (int)(indexf);
		index = std::max(index, 0);
		index = std::min(index, (int)buckets.size() - 1);
		if (buckets[index].alias < 0) {
			return index;
		}
		return u1 < buckets[index].height ? index : buckets[index].alias;
	}

	struct Bucket {
		float height = 0.0f;
		int alias = -1;
	};
	std::vector<float> probs;
	std::vector<Bucket> buckets;
};

AliasMethod aliasMethod;

//--------------------------------------------------------------
void ofApp::setup() {
	ofxRaccoonImGui::initialize();

	_camera.setNearClip(0.1f);
	_camera.setFarClip(100.0f);
	_camera.setDistance(5.0f);

}
void ofApp::exit() {
	ofxRaccoonImGui::shutdown();
}

//--------------------------------------------------------------
void ofApp::update() {
	aliasMethod.prepare(weights);
}

//--------------------------------------------------------------
void ofApp::draw(){
	ofEnableDepthTest();

	ofClear(0);

	_camera.begin();
	ofPushMatrix();
	ofRotateYDeg(90.0f);
	ofSetColor(64);
	ofDrawGridPlane(1.0f);
	ofPopMatrix();

	ofDrawAxis(50);
	

	for (int i = 0; i < aliasMethod.buckets.size(); ++i) {
		float height = aliasMethod.buckets[i].height;
		int alias = aliasMethod.buckets[i].alias;

		ofSetColor(128);
		ofRectangle bottom(i, 0, 1 /*w*/, height);
		ofDrawRectangle(bottom);

		ofSetColor(255, 0, 0);
		ofRectangle r_alias(i, height, 1, 1.0f - height);
		ofDrawRectangle(r_alias);

		ofSetColor(255);
		ofDrawBitmapString("[" + ofToString(i) + "]", bottom.getCenter());

		if (0 <= alias) {
			ofSetColor(255);
			ofDrawBitmapString("[" + ofToString(alias) + "]", r_alias.getCenter());
		}
	}

	auto drawCy = [](float x, float z, float h) {
		ofSetColor(200);
		ofDrawCylinder(glm::vec3(x, h * 0.5f, z), 0.3f, h);
		ofSetColor(0);
		ofNoFill();
		glEnable(GL_POLYGON_OFFSET_LINE);
		glPolygonOffset(-0.1f, 1.0f);
		ofDrawCylinder(glm::vec3(x, h * 0.5f, z), 0.3f, h);
		glDisable(GL_POLYGON_OFFSET_LINE);
		ofFill();
	};

	for (int i = 0; i < aliasMethod.buckets.size(); ++i) {
		float p = aliasMethod.probability(i);
		drawCy(i + 0.5f, 1.0f, p);

		ofSetColor(255, 0, 255);
		char buf[64];
		sprintf(buf, "[%.3f]", p);
		ofDrawBitmapString(buf, glm::vec3(i + 0.5f, p, 1.0f));
	}

	static rt::XoroshiroPlus128 random;
	std::vector<int> hist(aliasMethod.buckets.size());
	int N = 50000;
	for (int i = 0; i < N; ++i) {
		int index = aliasMethod.sample(random.uniform32f(), random.uniform32f());
		hist[index]++;
	}

	for (int i = 0; i < aliasMethod.buckets.size(); ++i) {
		float p = (float)hist[i] / N;
		drawCy(i + 0.5f, 2.0f, p);

		ofSetColor(255, 0, 255);
		char buf[64];
		sprintf(buf, "[%.3f]", p);
		ofDrawBitmapString(buf, glm::vec3(i + 0.5f, p, 2.0f));
	}

	_camera.end();

	ofDisableDepthTest();
	ofSetColor(255);

	ofxRaccoonImGui::ScopedImGui imgui;

	// camera control                                          for control clicked problem
	if (ImGui::IsWindowHovered(ImGuiHoveredFlags_AnyWindow) || (ImGui::IsWindowFocused(ImGuiFocusedFlags_AnyWindow) && ImGui::IsAnyMouseDown())) {
		_camera.disableMouseInput();
	}
	else {
		_camera.enableMouseInput();
	}

	ImGui::SetNextWindowPos(ImVec2(20, 20), ImGuiCond_Appearing);
	ImGui::SetNextWindowSize(ImVec2(300, 300), ImGuiCond_Appearing);
	ImGui::SetNextWindowCollapsed(false, ImGuiCond_Appearing);
	ImGui::SetNextWindowBgAlpha(0.5f);

	ImGui::Begin("settings", nullptr);

	int count = weights.size();
	ImGui::InputInt("N", &count);
	count = glm::clamp(count, 0, 10);

	weights.resize(count);

	for (int i = 0; i < weights.size(); ++i) {
		char label[16];
		sprintf(label, "w[%d]", i);
		ImGui::SliderFloat(label, &weights[i], 0.0f, 2.0f);
	}

	ImGui::End();
}

//--------------------------------------------------------------
void ofApp::keyPressed(int key){

}

//--------------------------------------------------------------
void ofApp::keyReleased(int key){

}

//--------------------------------------------------------------
void ofApp::mouseMoved(int x, int y ){

}

//--------------------------------------------------------------
void ofApp::mouseDragged(int x, int y, int button){

}

//--------------------------------------------------------------
void ofApp::mousePressed(int x, int y, int button){

}

//--------------------------------------------------------------
void ofApp::mouseReleased(int x, int y, int button){

}

//--------------------------------------------------------------
void ofApp::mouseEntered(int x, int y){

}

//--------------------------------------------------------------
void ofApp::mouseExited(int x, int y){

}

//--------------------------------------------------------------
void ofApp::windowResized(int w, int h){

}

//--------------------------------------------------------------
void ofApp::gotMessage(ofMessage msg){

}

//--------------------------------------------------------------
void ofApp::dragEvent(ofDragInfo dragInfo){ 

}
