function f = generate_filter (cycles, fudge_factor, increment, atten)

    if nargin != 4,
	    error ("Need four args.") ;
	    endif

    # Calculate N and make sure it is even.
    N = fix (4 * cycles * fudge_factor * increment) ;
    if rem (N, 2) != 0,
	    N = N - 1 ;
	    endif

    # Generate the Sinc function.
    m = -((N-1)/2):((N-1)/2) ;
    f = sinc (m / fudge_factor / increment) ;

    # Generate the window function and apply it.
    w = kaiser (N, (atten + 0.5) / 10) ;
    w = w' ;
    f = f .* w ;
    f = f / sum (f) ;

endfunction
