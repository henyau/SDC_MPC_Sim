#include "MPC.h"
#include <cppad/cppad.hpp>
#include <cppad/ipopt/solve.hpp>
#include <cfloat>
#include "Eigen-3.3/Eigen/Core"

using CppAD::AD;

// et the timestep length and duration
size_t N = 10;
double dt = 0.1;

// This value assumes the model presented in the classroom is used.
//
// It was obtained by measuring the radius formed by running the vehicle in the
// simulator around in a circle with a constant steering angle and velocity on a
// flat terrain.
//
// Lf was tuned until the the radius formed by the simulating the model
// presented in the classroom matched the previous radius.
//
// This is the length from front to CoG that has a similar radius.
const double Lf = 2.67;


size_t x_start = 0;
size_t y_start = x_start + N;
size_t psi_start = y_start + N;
size_t v_start = psi_start + N;
size_t cte_start = v_start + N;
size_t epsi_start = cte_start + N;
size_t delta_start = epsi_start + N;
size_t a_start = delta_start + N - 1;


class FG_eval {
 public:
  // Fitted polynomial coefficients
  Eigen::VectorXd coeffs;
  FG_eval(Eigen::VectorXd coeffs) { this->coeffs = coeffs; }

  typedef CPPAD_TESTVECTOR(AD<double>) ADvector;
  void operator()(ADvector& fg, const ADvector& vars) {
    // : implement MPC
    // `fg` a vector of the cost constraints, `vars` is a vector of variable values (state & actuators)
    // NOTE: You'll probably go back and forth between this function and
    // the Solver function below

    fg[0] = 0.0;    //cost functional
    double ref_v = 80*.44704; //ref velocity in m/s
    for(size_t i = 0; i< N; i++)
    {
    	fg[0] += 2.0*CppAD::pow(vars[cte_start+i],2);
	fg[0] += 5.0*CppAD::pow(vars[epsi_start+i],2);
	fg[0] += 5.0*CppAD::pow(vars[v_start+i]-ref_v,2);
    }

    // Minimize the use of actuators.
    for (size_t t = 0; t < N - 1; t++) {
      fg[0] += 1500*CppAD::pow(vars[delta_start + t], 2);
      fg[0] += 20*CppAD::pow(vars[a_start + t], 2);
    }

    // Minimize the value gap between sequential actuations.
    for (size_t t = 0; t < N - 2; t++) {
      fg[0] += 10*CppAD::pow(vars[delta_start + t + 1] - vars[delta_start + t], 2);
      fg[0] += 10*CppAD::pow(vars[a_start + t + 1] - vars[a_start + t], 2);
    }


    //model constraints
    fg[1 + x_start] = vars[x_start];	
    fg[1 + y_start] = vars[y_start];
    fg[1 + psi_start] = vars[psi_start];
    fg[1 + v_start] = vars[v_start];
    fg[1 + cte_start] = vars[cte_start];
    fg[1 + epsi_start] = vars[epsi_start];

    for(size_t i = 0; i<N-1; i++)
    {
    	AD<double> x0 = vars[x_start+i];
	AD<double> y0 = vars[y_start+i];
	AD<double> psi0 = vars[psi_start+i];
	AD<double> v0 = vars[v_start+i];
	AD<double> cte0 = vars[cte_start+i];
	AD<double> psie0 = vars[epsi_start+i];
	

	AD<double> x1 = vars[x_start+i+1];
	AD<double> y1 = vars[y_start+i+1];
	AD<double> psi1 = vars[psi_start+i+1];
	AD<double> v1 = vars[v_start+i+1];
	AD<double> cte1 = vars[cte_start+i+1];
	AD<double> psie1 = vars[epsi_start+i+1];
	
	AD<double> delta0 = vars[delta_start+i];
	AD<double> a0 = vars[a_start+i];

	if(i>1)
	{//time delay
		delta0 = vars[delta_start+i-1];
		a0 = vars[a_start+i-1];
	}
	AD<double> feval = coeffs[0]+coeffs[1]*x0+coeffs[2]*CppAD::pow(x0,2);
	AD<double> psides = CppAD::atan(coeffs[1]+2*coeffs[2]*x0);//angle desired using derivative of polynomial
	
	fg[x_start+i+2] = x1-(x0+v0*CppAD::cos(psi0)*dt);
	fg[y_start+i+2] = y1-(y0+v0*CppAD::sin(psi0)*dt); 
	fg[v_start+i+2] = v1-(v0+a0*dt);

	fg[psi_start+i+2] = psi1-(psi0-v0*delta0*dt/Lf);
 	fg[cte_start+i+2] = cte1-(feval-y0+v0*CppAD::sin(psie0)*dt);
	fg[epsi_start+i+2] = psie1-(psi0-psides-v0*delta0/Lf*dt);
   }
 
  }
};

//
// MPC class definition implementation.
//
MPC::MPC() {}
MPC::~MPC() {}

vector<double> MPC::Solve(Eigen::VectorXd state, Eigen::VectorXd coeffs) {
  bool ok = true;
//  size_t i;
  typedef CPPAD_TESTVECTOR(double) Dvector;

  // : Set the number of model variables (includes both states and inputs).
  // For example: If the state is a 4 element vector, the actuators is a 2
  // element vector and there are 10 timesteps. The number of variables is:
  //
  // 4 * 10 + 2 * 9
  size_t n_vars = 6*N+2*(N-1);
  // : Set the number of constraints
  size_t n_constraints = 6*N;

  // Initial value of the independent variables.
  // SHOULD BE 0 besides initial state.
  Dvector vars(n_vars);
  for (size_t i = 0; i < n_vars; i++) {
    vars[i] = 0;
  }
  vars[x_start] = state[0];
  vars[y_start] = state[1];
  vars[psi_start] = state[2];
  vars[v_start] = state[3];
  vars[cte_start] = state[4];
  vars[epsi_start] = state[5];


  Dvector vars_lowerbound(n_vars);
  Dvector vars_upperbound(n_vars);
  // : Set lower and upper limits for variables.

  //states
  for (size_t i = 0; i<delta_start; i++)
  {
    vars_lowerbound[i] = -DBL_MAX;
    vars_upperbound[i] = DBL_MAX;
  }

  //turning actuator limits
  for (size_t i = delta_start; i<a_start; i++)
  {
    vars_lowerbound[i] = -25*M_PI/180;
    vars_upperbound[i] = 25*M_PI/180;
  }
  //throttle limits
  for (size_t i = a_start; i<n_vars; i ++)
  {
    vars_lowerbound[i] = -1.0;
    vars_upperbound[i] = 1.0;
  }


  // Lower and upper limits for the constraints
  // Should be 0 besides initial state.
  Dvector constraints_lowerbound(n_constraints);
  Dvector constraints_upperbound(n_constraints);
  for (size_t i = 0; i < n_constraints; i++) {
    constraints_lowerbound[i] = 0;
    constraints_upperbound[i] = 0;
  }
  
  constraints_lowerbound[x_start] = state[0];
  constraints_lowerbound[y_start] = state[1];
  constraints_lowerbound[psi_start] =  state[2];
  constraints_lowerbound[v_start] = state[3];
  constraints_lowerbound[cte_start] =  state[4];
  constraints_lowerbound[epsi_start] =  state[5];
  
  constraints_upperbound[x_start] = state[0];
  constraints_upperbound[y_start] = state[1];
  constraints_upperbound[psi_start] =  state[2];
  constraints_upperbound[v_start] = state[3];
  constraints_upperbound[cte_start] =  state[4];
  constraints_upperbound[epsi_start] =  state[5];

  // object that computes objective and constraints
  FG_eval fg_eval(coeffs);

  //
  // NOTE: You don't have to worry about these options
  //
  // options for IPOPT solver
  std::string options;
  // Uncomment this if you'd like more print information
  options += "Integer print_level  0\n";
  // NOTE: Setting sparse to true allows the solver to take advantage
  // of sparse routines, this makes the computation MUCH FASTER. If you
  // can uncomment 1 of these and see if it makes a difference or not but
  // if you uncomment both the computation time should go up in orders of
  // magnitude.
  options += "Sparse  true        forward\n";
  options += "Sparse  true        reverse\n";
  // NOTE: Currently the solver has a maximum time limit of 0.5 seconds.
  // Change this as you see fit.
  options += "Numeric max_cpu_time          0.5\n";

  // place to return solution
  CppAD::ipopt::solve_result<Dvector> solution;

  // solve the problem
  CppAD::ipopt::solve<Dvector, FG_eval>(
      options, vars, vars_lowerbound, vars_upperbound, constraints_lowerbound,
      constraints_upperbound, fg_eval, solution);

  // Check some of the solution values
  ok &= solution.status == CppAD::ipopt::solve_result<Dvector>::success;

  // Cost
  auto cost = solution.obj_value;
  std::cout << "Cost " << cost << std::endl;

  // : Return the first actuator values. The variables can be accessed with
  // `solution.x[i]`.
  //
  // {...} is shorthand for creating a vector, so auto x1 = {1.0,2.0}
  // creates a 2 element double vector.
  vector<double> res;
  res.push_back(solution.x[delta_start]/(25*M_PI/180));
  res.push_back(solution.x[a_start]);
 //  res.push_back(solution.x[delta_start+1]/(25*M_PI/180));
 // res.push_back(solution.x[a_start+1]);
 
  for(size_t i = 0; i< N-1; i++)
  {
      res.push_back(solution.x[x_start+i]);
      res.push_back(solution.x[y_start+i]);
  }
  return res;
//  return {solution.x[delta_start]/(25*M_PI/180), solution.x[a_start]};
}
