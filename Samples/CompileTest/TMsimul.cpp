/*Yritetään toistaa pyDEC:illä tuotettuja simulaatiotuloksia

MPI ei toimi, tekee saman homman useammassa prosessissa. muuta kääntäjäasetuksia?
Kuinka voin debugatessa tarkastella bufferin lukuja ilman segventation faulttia?

koodipohjan ongelmia:
index out of bounds ei kaada ohjelmaa tai heitä varoitusta
väärädimensioisien matriisien kertominen ei kaada ohjelmaa tai heitä varoitusta
verkkojen tallennusmuoto ei ole järkevä, pitäisi olla ainakin numeropohjainen vaihtoehto

kommentit puuttuu lähes kaikkialta
testifunktiot puuttuu kaikkialta?
paljon turhaa toistoa
useita hyödyllisiä funktioita puuttuu, esimerkiksi funktioita ulkoisen verkon lukemiseen
*/




#include "../../GFD/Mesh/BuilderMesh.hpp"	//for building and modifying a mesh
#include "../../GFD/Mesh/PartMesh.hpp"	//for working with parts of a mesh?
#include "../../GFD/Types/Types.hpp"	//constants, shorter names for types, type couple
#include "../../GFD/Types/MpiEasy.hpp"	//for parallel computing
#include "../../GFD/Discrete/Dec.hpp"	//for dec related calculations
#include <iostream>
#include <fstream>
#include <chrono>

using namespace std;
using namespace gfd;

const uint space_steps = 5; // number of space elements per unit length
const uint time_steps = 4000; // number of time steps
const double dt = 0.003; //time step size

double getDouble(const std::string &str, const double def) {
	char *rest;
	const double val = strtod(str.c_str(), &rest);
	if(rest == NULL) return def;
	return val;
}
Buffer<double> getDoubles(const std::string &str) {
	Buffer<double> res;
	const string space = " \t";
	size_t pos = 0;
	pos = str.find_first_not_of(space, pos);
	while(pos != string::npos) {
		const size_t until = str.find_first_of(space, pos);
		res.push_back(getDouble(str.substr(pos, until), 0.0));
		pos = str.find_first_not_of(space, until);
	}
	return res;
}
uint getUint(const std::string &str, const uint def) {
	const double val = getDouble(str, double(def));
	if(val < 0.0) return 0;
	return uint(val + 0.5);
}
Buffer<uint> getUints(const std::string &str) {
	Buffer<uint> res;
	const string space = " \t";
	size_t pos = 0;
	pos = str.find_first_not_of(space, pos);
	while(pos != string::npos) {
		const size_t until = str.find_first_of(space, pos);
		res.push_back(getUint(str.substr(pos, until), 0));
		pos = str.find_first_not_of(space, until);
	}
	return res;
}

void line(){
	std::cout << "---------------"<<endl;
}
void loadMesh(PartMesh &mesh, string file, Buffer<double> &boundaryNodes){
    Text input;
    if(input.load(file)) std::cout << "mesh file loaded succesfully." << endl;
    while(input.hasRow()) {
		if(input.getRow().substr(0, 6).compare("$Nodes") == 0){
            std::cout << "reading nodes." << endl;
            break;
        } 
	}
    const uint numVert=getUint(input.getRow(),0);
    std::cout << numVert << endl;
    mesh.resizeNodeBuffer(numVert);
	for(uint i=0; i<numVert; i++) {
		const Buffer<double> pos = getDoubles(input.getRow());
		mesh.addNode(Vector4(pos[0], pos[1], 0.0, 0.0));
	}
    if(input.getRow().substr(0, 9).compare("$EndNodes") == 0) std::cout << "Nodes read succesfully." << endl;
	if(input.getRow().substr(0, 6).compare("$Faces") == 0){
        std::cout << "reading faces and edges." << endl;
    }
    const uint numFace=getUint(input.getRow(),0);
	Buffer<uint> flags(numFace*4);//record edges that are linked to only one face -> boundary edges
	flags.fill(0);
    for(uint i=0; i<numFace; i++) {
		const Buffer<uint> face = getUints(input.getRow());
		Buffer<uint> edge(face.size());
		for(uint i=0; i<face.size(); i++){
			edge[i] = mesh.addEdge(face[i%face.size()]-1, face[(i+1)%face.size()]-1);
			flags[edge[i]]++;
		}
		mesh.addFace(edge);
	}
	flags.resize(mesh.getEdgeSize());
	for(uint i=0; i<flags.size();i++){
		flags[i]=flags[i]%2;//flags[i]=1 iff edge number i is on the boundary
	}
	//for(uint i=0;i<flags.size();i++) cout << flags[i] << " ";
	Buffer<double> flagsV(mesh.getNodeSize());
	flagsV.fill(0.0);
	for(uint i=0; i<flags.size();i++){
		if(flags[i]==1){//flags[i]=1 iff node number i is on the boundary
			flagsV[mesh.getEdgeNodes(i)[0]]=1.0;
			flagsV[mesh.getEdgeNodes(i)[1]]=1.0;
			//mesh.setNodeFlag(mesh.getEdgeNodes(i)[0],1);
			//mesh.setNodeFlag(mesh.getEdgeNodes(i)[1],1);
		}
	}
	//for(uint i=0;i<flagsV.size();i++) cout << flagsV[i] << " ";
	boundaryNodes=flagsV;
    std::cout << numFace << endl;
    if(input.getRow().substr(0, 9).compare("$EndFaces") == 0) std::cout << "Faces and edges read succesfully." << endl;
}

int main(int argc, const char* argv[]) {
	initMPI();

	auto starttime = chrono::system_clock::now();

    PartMesh mesh(0, 1, 2);
	Buffer<double> boundaryNodes;
    if(argc==1){
        BuilderMesh bmesh(2);
        const double dx = 1.0 / double(space_steps);
        bmesh.createTriangleGrid(Vector2(0,0), Vector2(1,1), dx);
        mesh.swap(bmesh);
    }
    else{
        loadMesh(mesh,argv[1],boundaryNodes);
    }
	Text stat;
	mesh.writeStatistics(stat);
	stat.save("stats.txt");
    Dec dec(mesh, 0, mesh.getDimension());

	Diagonal<double> P1(boundaryNodes,0.0);
    Derivative d0; 
    d0=dec.integrateDerivative(fg_prim0, d0);
    Diagonal<double> h1;
    h1=dec.integrateHodge(HodgeUnit2, 0, fg_prim1, h1);//mistä tulee HodgeUnit2??
    Diagonal<double> h0i;
    h0i=dec.integrateHodge(HodgeUnit1, 0, fg_dual0, h0i);
    Column<double> h(0.0);
	dec.integrateZeroForm(fg_dual1, h);
	Column<double> e(0.0);
	dec.integrateZeroForm(fg_prim0, e);
    Sparse<double> delta;
	delta.setScale(dt, h0i*transpose(d0));
	Sparse<double> gamma;
	gamma.setScale(dt, h1*d0);
	P1.setScale(0.1*dt,h0i*P1);
	double solt[time_steps];

	std::cout << "Mesh and derivates generated in " << chrono::duration<double>(chrono::system_clock::now() - starttime).count() << " seconds" << endl;
	starttime = chrono::system_clock::now();
	uint kp = mesh.getNodeSize()/2;
    for(double i=0; i<time_steps; i++) {
		e+=delta*h;//-P1*e
        e.m_val[kp]=sin(PIx2*i/650.0);//60 158
        h-=gamma*e;
		solt[int(i+0.1)]=e.m_val[kp-5];//36 44
    }
	
	std::cout << "Timeintegration performed in " << chrono::duration<double>(chrono::system_clock::now() - starttime).count() << " seconds" << endl;
	starttime = chrono::system_clock::now();

	ofstream myfile;
	myfile.open ("solutions.txt");
	for(uint i=0;i<time_steps;i++){
		myfile << solt[i] << endl;
	}
	myfile.close();
    gfd::finalizeMPI();
}