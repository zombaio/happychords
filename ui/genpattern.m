function [pattern]=genpattern(stamp,d_max,pattern_size,density)
	density=density/prod(pattern_size);
	pattern=rand(pattern_size)<density;
	[y,x]=find(pattern);
	d=2*norm(0.5*size(stamp,1) , 0.5*size(stamp,2));
	pattern=zeros(pattern_size);
	h = waitbar(0,'Genrating pattern');
	wax = findobj(h, 'type','axes');
	tax = get(wax,'title');
	set(tax,'fontsize',15);
	hw=findobj(h,'type','patch');
	set(hw,'EdgeColor',[7.3e-2, 35.7e-2, 95.3e-2]...
		,'FaceColor',[97.6e-2, 33.3e-2, 65.9e-2]);
	for k=1:numel(y)
		B=imrotate(stamp,360*rand(),'bicubic','loose',0);
		B=double( (0.25 + 0.75*rand())*imresize(B,d_max*(0.5+0.5*rand())/d,'bicubic') )/255;
		range_y=mod(y(k):y(k) + size(B,1)-1,size(pattern,1))+1;
		range_x=mod(x(k):x(k) + size(B,2)-1,size(pattern,2))+1;
		pattern(range_y,range_x)=pattern(range_y,range_x).*(1-B) + B;
		waitbar (k/numel(y));
	end
end